/**
 * USBShark - Military-grade USB protocol analyzer
 * USB Interface Implementation
 */

#include "../include/usb_interface.h"
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

/* USB states and buffers */
static volatile usb_state_t usb_state = USB_STATE_DETACHED;
static volatile usb_monitor_config_t monitor_config;
static volatile bool monitoring_enabled = false;
static volatile bool bus_reset_detected = false;
static volatile uint8_t connected_devices = 0;

/* Timestamp counter using Timer1 */
static volatile uint32_t timestamp_counter = 0;

/* Ring buffer for USB packet data */
static ringbuffer_t usb_packet_buffer;
static uint8_t packet_data_buffer[64]; // Temporary storage for packet data

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
    
    // Initialize ring buffer
    ringbuffer_init(&usb_packet_buffer);
    
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
            }
        } else if (!dp_high && dm_high) {
            // Low speed device detected
            if (usb_state < USB_STATE_ATTACHED) {
                usb_state = USB_STATE_ATTACHED;
                connected_devices++;
            }
        } else if (!dp_high && !dm_high) {
            // No device or bus reset condition
            if (usb_state > USB_STATE_POWERED) {
                // This might be a bus reset
                bus_reset_detected = true;
            }
            usb_state = USB_STATE_POWERED;
        }
    } else {
        // USB power not detected
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
    // Copy configuration
    memcpy((void*)&monitor_config, config, sizeof(usb_monitor_config_t));
    
    // Reset timestamp counter
    usb_reset_timestamp();
    
    // Reset ring buffer
    ringbuffer_reset(&usb_packet_buffer);
    
    // Enable more detailed interrupt monitoring based on speed
    if (config->speed == USB_SPEED_LOW) {
        // Configure for low speed (1.5 Mbps)
        // Set appropriate timer/sampling rate
    } else {
        // Configure for full speed (12 Mbps)
        // Set appropriate timer/sampling rate
    }
    
    // Enable monitoring
    monitoring_enabled = true;
}

/**
 * Disable USB monitoring
 */
void usb_monitor_disable(void) {
    monitoring_enabled = false;
}

/**
 * Get the number of connected USB devices
 * @return Device count
 */
uint8_t usb_get_device_count(void) {
    return connected_devices;
}

/**
 * Capture a USB packet
 * @param packet Pointer to packet structure to fill
 * @return true if packet captured, false otherwise
 */
bool usb_capture_packet(usb_packet_t *packet) {
    // This is a placeholder for actual implementation
    // Currently just for testing basic USB detection
    
    if (!monitoring_enabled || ringbuffer_empty(&usb_packet_buffer)) {
        return false;
    }
    
    // In real implementation, we would reconstruct packets from the buffer
    // For now, we'll just return a dummy packet for testing
    packet->timestamp = usb_get_timestamp();
    packet->pid = USB_PID_SOF;  // Start of Frame packet
    packet->endpoint = 0;
    packet->dev_addr = 0;
    packet->data_len = 0;
    packet->data = NULL;
    packet->crc_valid = true;
    
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
        
        // Filter by direction if enabled
        if ((monitor_config.filter_in && packet->pid == USB_PID_IN) ||
            (monitor_config.filter_out && (packet->pid == USB_PID_OUT || packet->pid == USB_PID_SETUP))) {
            packet_matches = false;
        }
        
        // If packet passed filters, send to host
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
    // CRC-16 calculation for USB
    // In real implementation, this would compute proper USB CRC-16
    // For now, just a placeholder
    return true;
}

/* Interrupt Handlers */

/**
 * INT0 Interrupt handler for USB state changes
 */
ISR(INT0_vect) {
    // Detect edge transitions on D+/D-
    if (monitoring_enabled) {
        // In real implementation, this would capture individual bits
        // from the USB data stream using precise timing
        
        // For now, just detect bus state changes
        uint8_t pins = usb_read_data_pins();
        
        // Store to ring buffer for later processing
        ringbuffer_push(&usb_packet_buffer, pins);
    }
}

/**
 * Timer1 Overflow interrupt handler for timestamp maintenance
 */
ISR(TIMER1_OVF_vect) {
    // Each overflow is 65536 timer ticks
    timestamp_counter++;
} 