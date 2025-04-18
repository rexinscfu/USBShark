/**
 * USBShark - Military-grade USB protocol analyzer
 * Main Program Entry Point
 */

#include "../include/usb_interface.h"
#include "../include/usb_protocol.h"
#include "../include/ringbuffer.h"
#include "../include/comm_protocol.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

/* Status LED pins */
#define LED_POWER    PB0
#define LED_ACTIVITY PB1
#define LED_USB      PB2
#define LED_ERROR    PB3
#define LED_PORT     PORTB
#define LED_DDR      DDRB

/* Program states */
typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_MONITORING,
    STATE_ERROR
} program_state_t;

/* Global variables */
static volatile program_state_t current_state = STATE_INIT;
static volatile uint32_t activity_timestamp = 0;
static volatile uint8_t error_code = 0;
static volatile bool usb_activity = false;
static volatile uint32_t idle_counter = 0;
static volatile uint16_t buffer_usage = 0;
static volatile bool usb_reset_flag = false;

/* Monitoring configuration */
static usb_monitor_config_t default_config = {
    .speed = USB_SPEED_FULL,
    .capture_control = true,
    .capture_bulk = true,
    .capture_interrupt = true,
    .capture_isoc = true,
    .addr_filter = 0,           // No filter
    .ep_filter = 0,             // No filter
    .filter_in = false,
    .filter_out = false
};

/**
 * Initialize hardware
 */
static void hardware_init(void) {
    // Configure status LEDs as outputs
    LED_DDR |= (1 << LED_POWER) | (1 << LED_ACTIVITY) | (1 << LED_USB) | (1 << LED_ERROR);
    
    // Turn on power LED
    LED_PORT |= (1 << LED_POWER);
    
    // Initialize USB interface
    usb_init();
    
    // Initialize communication protocol
    comm_init();
    
    // Setup watchdog timer
    wdt_enable(WDTO_1S);
}

/**
 * Send initial status report to host
 */
static void send_initial_status(void) {
    // Get device count
    uint8_t device_count = usb_get_device_count();
    
    // Send status report
    comm_send_status_report(device_count, 0, 0);
}

/**
 * Handle incoming USB packets
 */
static void process_usb_packets(void) {
    usb_packet_t packet;
    uint8_t packets_processed = 0;
    
    // Process up to 10 packets at a time to avoid blocking too long
    while (packets_processed < 10 && usb_capture_packet(&packet)) {
        usb_process_packet(&packet);
        
        // Update activity indicator
        usb_activity = true;
        activity_timestamp = usb_get_timestamp();
        
        packets_processed++;
    }
}

/**
 * Handle command packets from host
 */
static void handle_command_packet(const comm_packet_t *packet) {
    switch (packet->type) {
        case PACKET_TYPE_CMD_RESET:
            // Reset monitoring state
            current_state = STATE_IDLE;
            usb_monitor_disable();
            comm_send_ack(packet->sequence);
            break;
            
        case PACKET_TYPE_CMD_START_CAPTURE:
            // Start capturing with default or provided config
            if (packet->length >= sizeof(usb_monitor_config_t)) {
                // Use configuration provided in packet
                usb_monitor_config_t config;
                memcpy(&config, packet->data, sizeof(usb_monitor_config_t));
                usb_monitor_enable(&config);
            } else {
                // Use default configuration
                usb_monitor_enable(&default_config);
            }
            current_state = STATE_MONITORING;
            comm_send_ack(packet->sequence);
            break;
            
        case PACKET_TYPE_CMD_STOP_CAPTURE:
            // Stop capturing
            usb_monitor_disable();
            current_state = STATE_IDLE;
            comm_send_ack(packet->sequence);
            break;
            
        case PACKET_TYPE_CMD_SET_FILTER:
            // Set packet filters
            if (packet->length >= sizeof(usb_monitor_config_t)) {
                // Update filter configuration
                memcpy(&default_config, packet->data, sizeof(usb_monitor_config_t));
                
                // Apply immediately if monitoring is active
                if (current_state == STATE_MONITORING) {
                    usb_monitor_enable(&default_config);
                }
            }
            comm_send_ack(packet->sequence);
            break;
            
        case PACKET_TYPE_CMD_GET_STATUS:
            // Send status report to host
            {
                uint8_t device_count = usb_get_device_count();
                uint8_t capture_state = (current_state == STATE_MONITORING) ? 1 : 0;
                comm_send_status_report(device_count, capture_state, buffer_usage);
                comm_send_ack(packet->sequence);
            }
            break;
            
        case PACKET_TYPE_CMD_SET_TIMESTAMP:
            // Reset timestamp counter
            if (packet->length >= 4) {
                // Could use timestamp from host, but for now just reset
                usb_reset_timestamp();
            }
            comm_send_ack(packet->sequence);
            break;
            
        default:
            // Unknown command
            comm_send_nack(packet->sequence, ERR_INVALID_COMMAND);
            break;
    }
}

/**
 * Update status LEDs
 */
static void update_leds(void) {
    // Activity LED - blink on USB traffic
    if (usb_activity) {
        LED_PORT |= (1 << LED_ACTIVITY);
        
        // Clear flag, will be set again if there's more activity
        usb_activity = false;
    } else if (usb_get_timestamp() - activity_timestamp > 100000) {
        // Turn off after 100ms of no activity
        LED_PORT &= ~(1 << LED_ACTIVITY);
    }
    
    // USB LED - on when devices connected
    if (usb_get_device_count() > 0) {
        LED_PORT |= (1 << LED_USB);
    } else {
        LED_PORT &= ~(1 << LED_USB);
    }
    
    // Error LED - on when in error state
    if (current_state == STATE_ERROR) {
        // Blink to indicate error code
        if ((idle_counter / 50000) % 10 < error_code) {
            LED_PORT |= (1 << LED_ERROR);
        } else {
            LED_PORT &= ~(1 << LED_ERROR);
        }
    } else {
        LED_PORT &= ~(1 << LED_ERROR);
    }
}

/**
 * Handle USB bus reset
 */
static void handle_usb_reset(void) {
    // In real implementation, this would reset the state of the monitored USB bus
    usb_reset_flag = false;
    
    // Flash the USB activity LED to indicate reset
    LED_PORT |= (1 << LED_ACTIVITY);
    _delay_ms(100);
    LED_PORT &= ~(1 << LED_ACTIVITY);
    _delay_ms(100);
    LED_PORT |= (1 << LED_ACTIVITY);
    _delay_ms(100);
    LED_PORT &= ~(1 << LED_ACTIVITY);
    
    // Send status update to host
    uint8_t device_count = usb_get_device_count();
    uint8_t capture_state = (current_state == STATE_MONITORING) ? 1 : 0;
    comm_send_status_report(device_count, capture_state, buffer_usage);
}

/**
 * Main program entry point
 */
int main(void) {
    // Initialize hardware
    hardware_init();
    
    // Enable interrupts
    sei();
    
    // Send initial status report
    send_initial_status();
    
    // Enter main loop
    current_state = STATE_IDLE;
    
    // Initialize default monitoring configuration
    default_config.speed = USB_SPEED_FULL;
    default_config.capture_control = true;
    default_config.capture_bulk = true;
    default_config.capture_interrupt = true;
    default_config.capture_isoc = true;
    default_config.addr_filter = 0;        // No filter
    default_config.ep_filter = 0;          // No filter
    default_config.filter_in = false;      // Don't filter IN transfers
    default_config.filter_out = false;     // Don't filter OUT transfers
    
    while (1) {
        // Reset watchdog
        wdt_reset();
        
        // Check for USB bus reset
        if (usb_reset_flag) {
            handle_usb_reset();
        }
        
        // Check USB bus state
        if (!usb_detect_bus_state()) {
            // No USB power detected, reset device count
            // This is handled in usb_detect_bus_state()
        }
        
        // Process any captured USB packets
        if (current_state == STATE_MONITORING) {
            process_usb_packets();
        }
        
        // Process any command packets from host
        comm_packet_t rx_packet;
        if (comm_receive_packet(&rx_packet)) {
            handle_command_packet(&rx_packet);
        }
        
        // Update status LEDs
        update_leds();
        
        // Update idle counter (used for timing certain operations)
        idle_counter++;
        
        // Send periodic status update (every ~1 second)
        if (idle_counter % 100000 == 0) {
            uint8_t device_count = usb_get_device_count();
            uint8_t capture_state = (current_state == STATE_MONITORING) ? 1 : 0;
            
            // Calculate buffer usage as percentage
            // In real implementation, this would measure actual buffer usage
            if (current_state == STATE_MONITORING) {
                // Simulate varying buffer usage for testing
                buffer_usage = (buffer_usage + 7) % 100;
            } else {
                buffer_usage = 0;
            }
            
            comm_send_status_report(device_count, capture_state, buffer_usage);
        }
    }
    
    return 0;  // Never reached
}

/**
 * Handle fatal errors
 * @param code Error code
 */
void handle_fatal_error(uint8_t code) {
    // Store error code
    error_code = code;
    
    // Enter error state
    current_state = STATE_ERROR;
    
    // Send error report to host
    comm_send_error(code, 0);
    
    // Turn on error LED
    LED_PORT |= (1 << LED_ERROR);
    
    // Continue in main loop for error visualization and possible recovery
}

/**
 * Watchdog timeout ISR
 */
ISR(WDT_vect) {
    // This is called if watchdog times out
    // In production code, we would attempt recovery or restart
    handle_fatal_error(ERR_TIMEOUT);
} 