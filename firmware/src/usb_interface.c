/**
 * USBShark - Military-grade USB protocol analyzer
 * USB Interface Implementation
 */

#include "../include/usb_interface.h"
#include "../include/usb_protocol.h"
#include "../include/ringbuffer.h"
#include "../include/comm_protocol.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

/* Hardware configuration (for Arduino Uno) 
 * USB D+ pin: PD2 (INT0)
 * USB D- pin: PD3
 * USB 5V sense pin: PC0 (ADC0)
 * USB Ground: GND
 */
#define USB_DP_PIN      PD2
#define USB_DM_PIN      PD3
#define USB_DP_PORT     PORTD
#define USB_DM_PORT     PORTD
#define USB_DP_DDR      DDRD
#define USB_DM_DDR      DDRD
#define USB_DP_PIN_REG  PIND
#define USB_DM_PIN_REG  PIND

/* USB voltage sensing via ADC */
#define USB_VSENSE_PIN  PC0
#define USB_VSENSE_PORT PORTC
#define USB_VSENSE_DDR  DDRC

/* USB bit timing (in CPU cycles) */
#define USB_FULL_SPEED_BIT_TIME  125  // 8MHz CPU / 12Mbps = 0.666us ≈ 5.33 cycles
#define USB_LOW_SPEED_BIT_TIME   1000 // 8MHz CPU / 1.5Mbps = 5.33us ≈ 42.7 cycles

/* USB Packet Buffer Size */
#define USB_MAX_PACKET_SIZE 64
#define USB_PACKET_BUFFER_SIZE 256

/* USB states and buffers */
static volatile usb_state_t usb_state = USB_STATE_DETACHED;
static volatile usb_monitor_config_t monitor_config;
static volatile bool monitoring_enabled = false;
static volatile bool bus_reset_detected = false;
static volatile uint8_t connected_devices = 0;
static volatile bool packet_in_progress = false;
static volatile uint8_t current_pid = 0;

/* Timestamp counter using Timer1 */
static volatile uint32_t timestamp_counter = 0;

/* Ring buffer for USB packet data */
static ringbuffer_t usb_packet_buffer;
static ringbuffer_t usb_event_buffer;

/* Temporary storage for packet data */
static uint8_t packet_data_buffer[USB_MAX_PACKET_SIZE];
static volatile uint8_t packet_data_length = 0;

/* USB transaction tracking */
static volatile uint8_t last_token_pid = 0;
static volatile uint8_t last_token_addr = 0;
static volatile uint8_t last_token_endp = 0;
static volatile uint32_t last_token_time = 0;
static volatile bool transaction_in_progress = false;

/* Transaction types for tracking */
typedef enum {
    TRANS_NONE = 0,
    TRANS_CONTROL_SETUP,
    TRANS_CONTROL_DATA,
    TRANS_CONTROL_STATUS,
    TRANS_BULK_IN,
    TRANS_BULK_OUT,
    TRANS_INTERRUPT_IN,
    TRANS_INTERRUPT_OUT,
    TRANS_ISOCHRONOUS
} transaction_type_t;

static volatile transaction_type_t current_transaction = TRANS_NONE;

/**
 * Initialize USB monitoring hardware
 */
void usb_init(void) {
    // Configure data pins as inputs with pullups initially
    USB_DP_DDR &= ~(1 << USB_DP_PIN);
    USB_DM_DDR &= ~(1 << USB_DM_PIN);
    USB_DP_PORT |= (1 << USB_DP_PIN);
    USB_DM_PORT |= (1 << USB_DM_PIN);
    
    // Configure voltage sense pin as input
    USB_VSENSE_DDR &= ~(1 << USB_VSENSE_PIN);
    
    // Initialize ring buffers
    ringbuffer_init(&usb_packet_buffer);
    ringbuffer_init(&usb_event_buffer);
    
    // Configure external interrupt on D+ for USB state change detection
    EICRA |= (1 << ISC00);    // Any logical change on INT0 generates interrupt
    EIMSK |= (1 << INT0);     // Enable INT0 interrupt
    
    // Configure Timer1 for timestamps (16-bit timer)
    TCCR1A = 0;                           // Normal mode
    TCCR1B = (1 << CS11) | (1 << CS10);   // Prescaler 64, ~4μs resolution at 16MHz
    TCNT1 = 0;                            // Reset counter
    TIMSK1 |= (1 << TOIE1);               // Enable overflow interrupt
    
    // Initialize ADC for voltage sensing
    ADMUX = (1 << REFS0) | (USB_VSENSE_PIN & 0x07); // AVCC reference, channel 0
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Enable ADC, prescaler 128
    
    // Initialize USB protocol module
    usb_protocol_init();
    
    // Initial bus state detection
    usb_detect_bus_state();
}

/**
 * Detect USB bus state by checking voltage on D+ and D-
 * @return true if bus powered, false otherwise
 */
bool usb_detect_bus_state(void) {
    bool bus_powered = false;
    
    // Start ADC conversion
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)); // Wait for conversion to complete
    
    // Check if USB voltage is present (>4V - ADC value ~800 at 5V reference)
    if (ADC > 800) {
        bus_powered = true;
        
        // Check for device presence by examining D+ and D- states
        uint8_t data_pins = usb_read_data_pins();
        bool dp_high = data_pins & (1 << USB_DP_PIN);
        bool dm_high = data_pins & (1 << USB_DM_PIN);
        
        // Full speed device pulls D+ high, low speed pulls D- high
        if (dp_high && !dm_high) {
            // Full speed device detected
            if (usb_state < USB_STATE_ATTACHED) {
                usb_state = USB_STATE_ATTACHED;
                connected_devices++;
                
                // Send device connection event to host
                uint8_t event_data[2] = {1, USB_SPEED_FULL};
                comm_send_packet(PACKET_TYPE_USB_STATE_CHANGE, event_data, 2);
            }
        } else if (!dp_high && dm_high) {
            // Low speed device detected
            if (usb_state < USB_STATE_ATTACHED) {
                usb_state = USB_STATE_ATTACHED;
                connected_devices++;
                
                // Send device connection event to host
                uint8_t event_data[2] = {1, USB_SPEED_LOW};
                comm_send_packet(PACKET_TYPE_USB_STATE_CHANGE, event_data, 2);
            }
        } else if (!dp_high && !dm_high) {
            // No device or bus reset condition
            if (usb_state > USB_STATE_POWERED) {
                // This might be a bus reset
                bus_reset_detected = true;
                
                // Send bus reset event to host
                uint8_t event_data[1] = {2}; // 2 = reset
                comm_send_packet(PACKET_TYPE_USB_STATE_CHANGE, event_data, 1);
            }
            usb_state = USB_STATE_POWERED;
        }
    } else {
        // USB power not detected
        if (usb_state > USB_STATE_DETACHED) {
            // Device was disconnected
            // Send device disconnection event to host
            uint8_t event_data[1] = {0}; // 0 = disconnected
            comm_send_packet(PACKET_TYPE_USB_STATE_CHANGE, event_data, 1);
        }
        
        usb_state = USB_STATE_DETACHED;
        connected_devices = 0;
    }
    
    return bus_powered;
}

/**
 * Enable USB monitoring with given configuration
 * @param config Monitoring configuration
 */
void usb_monitor_enable(const usb_monitor_config_t *config) {
    uint8_t sreg = SREG;
    cli();
    
    // Copy configuration
    memcpy((void*)&monitor_config, config, sizeof(usb_monitor_config_t));
    
    // Reset timestamp counter
    usb_reset_timestamp();
    
    // Reset ring buffers
    ringbuffer_reset(&usb_packet_buffer);
    ringbuffer_reset(&usb_event_buffer);
    
    // Reset transaction tracking
    last_token_pid = 0;
    last_token_addr = 0;
    last_token_endp = 0;
    last_token_time = 0;
    transaction_in_progress = false;
    current_transaction = TRANS_NONE;
    
    // Configure external interrupt for edge detection
    if (config->speed == USB_SPEED_LOW) {
        // Set for low-speed (1.5 Mbps) - 666.7 ns per bit
        // Need slower edge detection
        EICRA = (EICRA & ~((1 << ISC01) | (1 << ISC00))) | (1 << ISC00); // Any logical change
    } else {
        // Set for full-speed (12 Mbps) - 83.3 ns per bit
        // Need faster edge detection
        EICRA = (EICRA & ~((1 << ISC01) | (1 << ISC00))) | (1 << ISC00); // Any logical change
    }
    
    // Enable monitoring
    monitoring_enabled = true;
    
    SREG = sreg;
}

/**
 * Disable USB monitoring
 */
void usb_monitor_disable(void) {
    uint8_t sreg = SREG;
    cli();
    
    monitoring_enabled = false;
    
    SREG = sreg;
}

/**
 * Get the number of connected USB devices
 * @return Device count
 */
uint8_t usb_get_device_count(void) {
    return connected_devices;
}

/**
 * Process a packet from the buffer
 * @param raw_data Pointer to raw packet data
 * @param length Length of raw data
 * @return true if successful
 */
static bool process_raw_packet(const uint8_t *raw_data, uint8_t length) {
    if (length < 1) {
        return false;
    }
    
    // Create a packet structure
    usb_packet_t packet;
    packet.timestamp = usb_get_timestamp();
    
    // Decode the packet
    if (!usb_decode_packet(raw_data, length, &packet)) {
        return false;
    }
    
    // Track transactions
    if (usb_is_token_packet(packet.pid)) {
        // Store token details for later correlation
        last_token_pid = packet.pid;
        last_token_addr = packet.dev_addr;
        last_token_endp = packet.endpoint;
        last_token_time = packet.timestamp;
        
        if (packet.pid == USB_PID_SETUP) {
            current_transaction = TRANS_CONTROL_SETUP;
        } else if (packet.pid == USB_PID_IN) {
            // Determine transaction type based on endpoint
            // This is simplified - in reality would need endpoint descriptor info
            if (packet.endpoint == 0) {
                current_transaction = TRANS_CONTROL_DATA;
            } else {
                current_transaction = TRANS_BULK_IN;
            }
        } else if (packet.pid == USB_PID_OUT) {
            if (packet.endpoint == 0) {
                current_transaction = TRANS_CONTROL_DATA;
            } else {
                current_transaction = TRANS_BULK_OUT;
            }
        }
        
        transaction_in_progress = true;
    }
    else if (usb_is_data_packet(packet.pid)) {
        // Data phase of a transaction
        if (transaction_in_progress) {
            // Correlate with previous token
            packet.dev_addr = last_token_addr;
            packet.endpoint = last_token_endp;
            
            // Special handling for SETUP data
            if (current_transaction == TRANS_CONTROL_SETUP && packet.data_len == 8) {
                // This is a setup packet - decode control request
                usb_setup_packet_t setup;
                usb_decode_setup_packet(packet.data, &setup);
                
                // Additional processing based on setup packet could be done here
            }
        }
    }
    else if (usb_is_handshake_packet(packet.pid)) {
        // Handshake phase of a transaction
        if (transaction_in_progress) {
            // Correlate with previous token
            packet.dev_addr = last_token_addr;
            packet.endpoint = last_token_endp;
            
            // Transaction is complete
            transaction_in_progress = false;
            current_transaction = TRANS_NONE;
        }
    }
    
    // Process the packet through the filter pipeline
    usb_process_packet(&packet);
    
    return true;
}

/**
 * Capture a USB packet
 * @param packet Pointer to packet structure to fill
 * @return true if packet captured, false otherwise
 */
bool usb_capture_packet(usb_packet_t *packet) {
    uint8_t data[USB_MAX_PACKET_SIZE];
    uint8_t length = 0;
    
    // Check if buffer has data
    if (ringbuffer_empty(&usb_packet_buffer)) {
        return false;
    }
    
    // Read packet type (PID)
    if (!ringbuffer_pop(&usb_packet_buffer, &data[0])) {
        return false;
    }
    
    length = 1;
    
    // Determine packet type and expected length
    uint8_t pid = data[0];
    uint8_t expected_length = 0;
    
    if (usb_is_token_packet(pid)) {
        expected_length = 3; // PID + 2 bytes (addr, endp, crc)
    } else if (usb_is_data_packet(pid)) {
        // Data packets have variable length, read length byte
        uint8_t data_length;
        if (!ringbuffer_pop(&usb_packet_buffer, &data_length)) {
            return false;
        }
        
        // Read data payload
        for (uint8_t i = 0; i < data_length && i < USB_MAX_PACKET_SIZE - 3; i++) {
            if (!ringbuffer_pop(&usb_packet_buffer, &data[length++])) {
                return false;
            }
        }
        
        // Read CRC16 (2 bytes)
        for (uint8_t i = 0; i < 2 && length < USB_MAX_PACKET_SIZE; i++) {
            if (!ringbuffer_pop(&usb_packet_buffer, &data[length++])) {
                return false;
            }
        }
    } else if (usb_is_handshake_packet(pid)) {
        // Handshake packets have only PID
        expected_length = 1;
    } else {
        // Unknown packet type
        return false;
    }
    
    // Check if we have a valid packet
    if (length < expected_length) {
        return false;
    }
    
    // Process the raw packet
    if (!process_raw_packet(data, length)) {
        return false;
    }
    
    // Fill in the output packet structure
    packet->timestamp = usb_get_timestamp();
    packet->pid = pid;
    
    // The rest of the fields are filled by process_raw_packet
    // via usb_decode_packet
    
    return true;
}

/**
 * Process a captured USB packet
 * @param packet Pointer to the packet to process
 */
void usb_process_packet(const usb_packet_t *packet) {
    // Check if packet matches filters
    if (monitoring_enabled) {
        // Apply filter logic here
        bool packet_matches = true;
        
        // Filter by device address if enabled
        if (monitor_config.addr_filter != 0 && 
            packet->dev_addr != monitor_config.addr_filter) {
            packet_matches = false;
        }
        
        // Filter by endpoint if enabled
        if (monitor_config.ep_filter != 0 && 
            packet->endpoint != monitor_config.ep_filter) {
            packet_matches = false;
        }
        
        // Filter by transfer type
        bool is_control = (packet->endpoint == 0);
        bool is_interrupt = false; // Would need endpoint descriptor info
        bool is_bulk = !is_control && !is_interrupt;
        bool is_isoc = false; // Would need endpoint descriptor info
        
        if (!monitor_config.capture_control && is_control) {
            packet_matches = false;
        }
        
        if (!monitor_config.capture_bulk && is_bulk) {
            packet_matches = false;
        }
        
        if (!monitor_config.capture_interrupt && is_interrupt) {
            packet_matches = false;
        }
        
        if (!monitor_config.capture_isoc && is_isoc) {
            packet_matches = false;
        }
        
        // Filter by direction if enabled
        if ((monitor_config.filter_in && packet->pid == USB_PID_IN) ||
            (monitor_config.filter_out && (packet->pid == USB_PID_OUT || packet->pid == USB_PID_SETUP))) {
            packet_matches = false;
        }
        
        // If packet passed all filters, send to host
        if (packet_matches) {
            usb_send_packet_to_host(packet);
        }
    }
}

/**
 * Send captured packet to host computer
 * @param packet Pointer to the packet to send
 */
void usb_send_packet_to_host(const usb_packet_t *packet) {
    // Prepare packet data for transmission
    uint8_t buffer[8 + packet->data_len];
    
    // Timestamp (4 bytes)
    buffer[0] = (packet->timestamp >> 24) & 0xFF;
    buffer[1] = (packet->timestamp >> 16) & 0xFF;
    buffer[2] = (packet->timestamp >> 8) & 0xFF;
    buffer[3] = packet->timestamp & 0xFF;
    
    // PID (1 byte)
    buffer[4] = packet->pid;
    
    // Device address and endpoint (1 byte each)
    buffer[5] = packet->dev_addr;
    buffer[6] = packet->endpoint;
    
    // CRC status (1 bit) and reserved (7 bits)
    buffer[7] = packet->crc_valid ? 0x80 : 0x00;
    
    // Copy data if present
    if (packet->data_len > 0 && packet->data != NULL) {
        memcpy(&buffer[8], packet->data, packet->data_len);
    }
    
    // Send to host via communication protocol
    comm_send_packet(PACKET_TYPE_USB_PACKET, buffer, 8 + packet->data_len);
}

/**
 * Set D+ pin as input
 */
void usb_dp_set_input(void) {
    USB_DP_DDR &= ~(1 << USB_DP_PIN);
}

/**
 * Set D+ pin as output
 */
void usb_dp_set_output(void) {
    USB_DP_DDR |= (1 << USB_DP_PIN);
}

/**
 * Set D- pin as input
 */
void usb_dm_set_input(void) {
    USB_DM_DDR &= ~(1 << USB_DM_PIN);
}

/**
 * Set D- pin as output
 */
void usb_dm_set_output(void) {
    USB_DM_DDR |= (1 << USB_DM_PIN);
}

/**
 * Read state of D+ and D- pins
 * @return Byte containing pin states
 */
uint8_t usb_read_data_pins(void) {
    return USB_DP_PIN_REG;
}

/**
 * Get current timestamp
 * @return microsecond timestamp
 */
uint32_t usb_get_timestamp(void) {
    uint32_t time;
    uint16_t timer_count;
    
    // Atomic read of volatile variables
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        time = timestamp_counter;
        timer_count = TCNT1;
    }
    
    // Convert timer ticks to microseconds (with 64 prescaler at 16MHz, each tick is 4µs)
    return (time << 16) | timer_count;
}

/**
 * Reset timestamp counter
 */
void usb_reset_timestamp(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        timestamp_counter = 0;
        TCNT1 = 0;
    }
}

/**
 * Check USB packet CRC
 * @param data Buffer containing packet data
 * @param length Length of data
 * @param crc CRC value to check against
 * @return true if CRC valid, false otherwise
 */
bool usb_crc_check(const uint8_t *data, uint16_t length, uint16_t crc) {
    return usb_calculate_crc16(data, length) == crc;
}

/* Interrupt Handlers */

/**
 * INT0 Interrupt handler for USB state changes
 */
ISR(INT0_vect) {
    // Get pin states
    uint8_t pins = usb_read_data_pins();
    bool dp_state = pins & (1 << USB_DP_PIN);
    bool dm_state = pins & (1 << USB_DM_PIN);
    
    // Record the timestamp
    uint32_t timestamp = usb_get_timestamp();
    
    if (monitoring_enabled) {
        // Simple edge detection to identify packet start/end conditions
        static bool last_dp_state = false;
        static bool last_dm_state = false;
        static uint32_t last_edge_time = 0;
        static uint8_t bit_count = 0;
        static uint8_t current_byte = 0;
        static bool sync_detected = false;
        
        // Calculate time since last edge
        uint32_t time_diff = timestamp - last_edge_time;
        last_edge_time = timestamp;
        
        // Edge detection
        bool dp_edge = dp_state != last_dp_state;
        bool dm_edge = dm_state != last_dm_state;
        last_dp_state = dp_state;
        last_dm_state = dm_state;
        
        // If both lines are low for a long time, it's a SE0 (bus reset or EOP)
        if (!dp_state && !dm_state) {
            // SE0 condition - end of packet or bus reset
            if (time_diff > 20) {  // More than ~2.5 bit times
                // This is probably an EOP (End of Packet) or bus reset
                if (packet_in_progress) {
                    packet_in_progress = false;
                    
                    // Store the packet in the buffer if we have data
                    if (packet_data_length > 0) {
                        // Push PID
                        ringbuffer_push(&usb_packet_buffer, current_pid);
                        
                        // Push data length
                        ringbuffer_push(&usb_packet_buffer, packet_data_length);
                        
                        // Push packet data
                        for (uint8_t i = 0; i < packet_data_length; i++) {
                            ringbuffer_push(&usb_packet_buffer, packet_data_buffer[i]);
                        }
                        
                        // Reset for next packet
                        packet_data_length = 0;
                    }
                }
                
                if (time_diff > 250) {  // More than ~10 bit times
                    // This is probably a bus reset
                    bus_reset_detected = true;
                    
                    // Send bus reset event to host
                    uint8_t event_data[1] = {2}; // 2 = reset
                    comm_send_packet(PACKET_TYPE_USB_STATE_CHANGE, event_data, 1);
                }
                
                // Reset packet detection state
                sync_detected = false;
                bit_count = 0;
                current_byte = 0;
            }
        }
        else if (dp_edge || dm_edge) {
            // Edge detected during data transmission
            if (!sync_detected) {
                // Look for SYNC pattern (00000001b)
                if (dp_state && !dm_state && bit_count == 7) {
                    // SYNC pattern detected, start collecting data
                    sync_detected = true;
                    packet_in_progress = true;
                    bit_count = 0;
                    current_byte = 0;
                } else {
                    // Continue building SYNC
                    bit_count = (bit_count + 1) % 8;
                }
            } else if (packet_in_progress) {
                // Process data bits
                if (dp_state != dm_state) {  // Differential '1'
                    current_byte |= (1 << bit_count);
                }
                
                bit_count++;
                
                if (bit_count == 8) {
                    // Complete byte received
                    if (packet_data_length == 0) {
                        // This is the PID
                        current_pid = current_byte;
                    } else if (packet_data_length < USB_MAX_PACKET_SIZE) {
                        // Store data byte
                        packet_data_buffer[packet_data_length - 1] = current_byte;
                    }
                    
                    packet_data_length++;
                    bit_count = 0;
                    current_byte = 0;
                }
            }
        }
    }
}

/**
 * Timer1 Overflow interrupt handler for timestamp maintenance
 */
ISR(TIMER1_OVF_vect) {
    // Each overflow is 65536 timer ticks
    timestamp_counter++;
} 