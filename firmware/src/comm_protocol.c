/**
 * USBShark - Military-grade USB protocol analyzer
 * Communication Protocol Implementation
 */

#include "../include/comm_protocol.h"
#include "../include/ringbuffer.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

/* UART settings */
#define UART_BAUD 1000000UL  // 1 Mbps for high-speed data transfer
#define UBRR_VALUE ((F_CPU / (UART_BAUD * 16UL)) - 1)

/* Protocol state machine */
typedef enum {
    PROTO_STATE_WAIT_SYNC,
    PROTO_STATE_TYPE,
    PROTO_STATE_LENGTH,
    PROTO_STATE_SEQUENCE,
    PROTO_STATE_DATA,
    PROTO_STATE_CRC_HIGH,
    PROTO_STATE_CRC_LOW
} proto_state_t;

/* Ring buffers for UART TX and RX */
static ringbuffer_t uart_tx_buffer;
static ringbuffer_t uart_rx_buffer;

/* Protocol state variables */
static volatile proto_state_t rx_state = PROTO_STATE_WAIT_SYNC;
static volatile uint8_t rx_data_count = 0;
static volatile uint8_t rx_packet_length = 0;
static volatile uint8_t tx_sequence = 0;
static volatile uint8_t rx_sequence = 0;
static comm_packet_t rx_packet;

/* CRC-16 lookup table */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

/**
 * Initialize communication module
 */
void comm_init(void) {
    // Initialize ring buffers
    ringbuffer_init(&uart_tx_buffer);
    ringbuffer_init(&uart_rx_buffer);
    
    // Configure UART
    // Set baud rate
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)UBRR_VALUE;
    
    // Enable receiver, transmitter and RX Complete interrupt
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    
    // Set frame format: 8 data bits, 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    
    // Initialize protocol state
    rx_state = PROTO_STATE_WAIT_SYNC;
    tx_sequence = 0;
    rx_sequence = 0;
}

/**
 * Send byte to UART
 * @param data Byte to send
 * @return true if successful, false if buffer full
 */
static bool uart_send_byte(uint8_t data) {
    // If UART data register is empty and TX buffer is empty, send directly
    if ((UCSR0A & (1 << UDRE0)) && ringbuffer_empty(&uart_tx_buffer)) {
        UDR0 = data;
        return true;
    }
    
    // Otherwise, add to TX buffer
    uint8_t buffer_status;
    
    // Disable interrupts while modifying buffer
    uint8_t sreg = SREG;
    cli();
    
    buffer_status = ringbuffer_push(&uart_tx_buffer, data);
    
    // If this is the first byte, enable TX Complete interrupt
    if (buffer_status && ringbuffer_count(&uart_tx_buffer) == 1) {
        UCSR0B |= (1 << UDRIE0);
    }
    
    // Restore interrupt state
    SREG = sreg;
    
    return buffer_status;
}

/**
 * Send packet to host
 * @param type Packet type
 * @param data Packet data
 * @param length Data length
 * @return true if successful, false if transmission failed
 */
bool comm_send_packet(packet_type_t type, const uint8_t *data, uint8_t length) {
    // Check if length is valid
    if (length > COMM_MAX_PACKET_SIZE) {
        return false;
    }
    
    // Calculate CRC of header and data
    uint8_t header[3] = {type, length, tx_sequence};
    uint16_t crc = comm_calculate_crc(header, 3);
    crc = comm_calculate_crc_continue(data, length, crc);
    
    // Send sync byte (without escaping)
    if (!uart_send_byte(COMM_SYNC_BYTE)) {
        return false;
    }
    
    // Send packet type with escaping
    if (type == COMM_SYNC_BYTE || type == COMM_ESCAPE_BYTE) {
        if (!uart_send_byte(COMM_ESCAPE_BYTE)) return false;
        if (!uart_send_byte(type ^ 0xFF)) return false;
    } else {
        if (!uart_send_byte(type)) return false;
    }
    
    // Send length with escaping
    if (length == COMM_SYNC_BYTE || length == COMM_ESCAPE_BYTE) {
        if (!uart_send_byte(COMM_ESCAPE_BYTE)) return false;
        if (!uart_send_byte(length ^ 0xFF)) return false;
    } else {
        if (!uart_send_byte(length)) return false;
    }
    
    // Send sequence with escaping
    if (tx_sequence == COMM_SYNC_BYTE || tx_sequence == COMM_ESCAPE_BYTE) {
        if (!uart_send_byte(COMM_ESCAPE_BYTE)) return false;
        if (!uart_send_byte(tx_sequence ^ 0xFF)) return false;
    } else {
        if (!uart_send_byte(tx_sequence)) return false;
    }
    
    // Send data with escaping
    for (uint8_t i = 0; i < length; i++) {
        if (data[i] == COMM_SYNC_BYTE || data[i] == COMM_ESCAPE_BYTE) {
            if (!uart_send_byte(COMM_ESCAPE_BYTE)) return false;
            if (!uart_send_byte(data[i] ^ 0xFF)) return false;
        } else {
            if (!uart_send_byte(data[i])) return false;
        }
    }
    
    // Send CRC with escaping
    uint8_t crc_high = (crc >> 8) & 0xFF;
    uint8_t crc_low = crc & 0xFF;
    
    if (crc_high == COMM_SYNC_BYTE || crc_high == COMM_ESCAPE_BYTE) {
        if (!uart_send_byte(COMM_ESCAPE_BYTE)) return false;
        if (!uart_send_byte(crc_high ^ 0xFF)) return false;
    } else {
        if (!uart_send_byte(crc_high)) return false;
    }
    
    if (crc_low == COMM_SYNC_BYTE || crc_low == COMM_ESCAPE_BYTE) {
        if (!uart_send_byte(COMM_ESCAPE_BYTE)) return false;
        if (!uart_send_byte(crc_low ^ 0xFF)) return false;
    } else {
        if (!uart_send_byte(crc_low)) return false;
    }
    
    // Increment sequence number
    tx_sequence++;
    
    return true;
}

/**
 * Process received packet
 * @param packet Received packet
 * @return true if packet processed successfully
 */
bool comm_process_command(const comm_packet_t *packet) {
    bool result = false;
    
    // Process based on packet type
    switch (packet->type) {
        case PACKET_TYPE_CMD_RESET:
            // Handle system reset
            // To be implemented
            result = true;
            break;
            
        case PACKET_TYPE_CMD_START_CAPTURE:
            // Start USB capture with config in packet data
            // To be implemented
            result = true;
            break;
            
        case PACKET_TYPE_CMD_STOP_CAPTURE:
            // Stop USB capture
            // To be implemented
            result = true;
            break;
            
        case PACKET_TYPE_CMD_SET_FILTER:
            // Set USB packet filter
            // To be implemented
            result = true;
            break;
            
        case PACKET_TYPE_CMD_GET_STATUS:
            // Return system status
            // To be implemented
            result = true;
            break;
            
        case PACKET_TYPE_CMD_SET_TIMESTAMP:
            // Set timestamp value
            // To be implemented
            result = true;
            break;
            
        case PACKET_TYPE_CMD_SET_CONFIG:
            // Set system configuration
            // To be implemented
            result = true;
            break;
            
        default:
            // Unknown command
            comm_send_nack(packet->sequence, ERR_INVALID_COMMAND);
            result = false;
            break;
    }
    
    // Send acknowledgment for successful commands
    if (result) {
        comm_send_ack(packet->sequence);
    }
    
    return result;
}

/**
 * Send acknowledgment
 * @param sequence Sequence number to acknowledge
 */
void comm_send_ack(uint8_t sequence) {
    uint8_t ack_data = sequence;
    comm_send_packet(PACKET_TYPE_ACK, &ack_data, 1);
}

/**
 * Send negative acknowledgment
 * @param sequence Sequence number to acknowledge
 * @param error Error code
 */
void comm_send_nack(uint8_t sequence, error_code_t error) {
    uint8_t nack_data[2] = {sequence, error};
    comm_send_packet(PACKET_TYPE_NACK, nack_data, 2);
}

/**
 * Calculate CRC-16 using CCITT polynomial
 * @param data Data buffer
 * @param length Data length
 * @return CRC-16 value
 */
uint16_t comm_calculate_crc(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    
    return crc;
}

/**
 * Continue CRC calculation from previous CRC
 * @param data Data buffer
 * @param length Data length
 * @param crc Previous CRC value
 * @return Updated CRC-16 value
 */
uint16_t comm_calculate_crc_continue(const uint8_t *data, uint16_t length, uint16_t crc) {
    for (uint16_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    
    return crc;
}

/**
 * Escape data for transmission
 * @param dest Destination buffer
 * @param src Source buffer
 * @param length Source length
 * @param escaped_length Pointer to store resulting length
 */
void comm_escape_data(uint8_t *dest, const uint8_t *src, uint8_t length, uint8_t *escaped_length) {
    uint8_t out_idx = 0;
    
    for (uint8_t i = 0; i < length; i++) {
        if (src[i] == COMM_SYNC_BYTE || src[i] == COMM_ESCAPE_BYTE) {
            dest[out_idx++] = COMM_ESCAPE_BYTE;
            dest[out_idx++] = src[i] ^ 0xFF;
        } else {
            dest[out_idx++] = src[i];
        }
    }
    
    *escaped_length = out_idx;
}

/**
 * Unescape received data
 * @param dest Destination buffer
 * @param src Source buffer
 * @param length Source length
 * @param unescaped_length Pointer to store resulting length
 * @return true if successful, false if invalid escape sequence
 */
bool comm_unescape_data(uint8_t *dest, const uint8_t *src, uint8_t length, uint8_t *unescaped_length) {
    uint8_t out_idx = 0;
    bool escape_next = false;
    
    for (uint8_t i = 0; i < length; i++) {
        if (escape_next) {
            dest[out_idx++] = src[i] ^ 0xFF;
            escape_next = false;
        } else if (src[i] == COMM_ESCAPE_BYTE) {
            escape_next = true;
        } else {
            dest[out_idx++] = src[i];
        }
    }
    
    // If we end with an escape character, it's an error
    if (escape_next) {
        return false;
    }
    
    *unescaped_length = out_idx;
    return true;
}

/**
 * Send USB packet to host
 * @param data USB packet data
 * @param length Data length
 * @param timestamp Packet timestamp
 * @param pid USB PID
 * @return true if successful
 */
bool comm_send_usb_packet(const uint8_t *data, uint8_t length, uint32_t timestamp, uint8_t pid) {
    // Create a buffer for the packet data
    uint8_t packet_data[8 + length];
    
    // Add timestamp (4 bytes, big endian)
    packet_data[0] = (timestamp >> 24) & 0xFF;
    packet_data[1] = (timestamp >> 16) & 0xFF;
    packet_data[2] = (timestamp >> 8) & 0xFF;
    packet_data[3] = timestamp & 0xFF;
    
    // Add PID
    packet_data[4] = pid;
    
    // Add reserved bytes
    packet_data[5] = 0;
    packet_data[6] = 0;
    packet_data[7] = 0;
    
    // Add packet data
    if (length > 0) {
        memcpy(&packet_data[8], data, length);
    }
    
    // Send packet
    return comm_send_packet(PACKET_TYPE_USB_PACKET, packet_data, 8 + length);
}

/**
 * Send status report to host
 * @param device_count Number of connected devices
 * @param capture_state Current capture state
 * @param buffer_usage Buffer usage percentage
 */
void comm_send_status_report(uint8_t device_count, uint8_t capture_state, uint16_t buffer_usage) {
    uint8_t status_data[4];
    
    status_data[0] = device_count;
    status_data[1] = capture_state;
    status_data[2] = (buffer_usage >> 8) & 0xFF;
    status_data[3] = buffer_usage & 0xFF;
    
    comm_send_packet(PACKET_TYPE_STATUS_REPORT, status_data, 4);
}

/**
 * Send error report to host
 * @param error_code Error code
 * @param context Context information
 */
void comm_send_error(error_code_t error_code, uint8_t context) {
    uint8_t error_data[2];
    
    error_data[0] = error_code;
    error_data[1] = context;
    
    comm_send_packet(PACKET_TYPE_ERROR_REPORT, error_data, 2);
}

/**
 * Process incoming bytes according to protocol state machine
 * @param byte Received byte
 */
static void process_rx_byte(uint8_t byte) {
    static bool escape_next = false;
    static uint8_t unescaped_byte;
    
    // Handle escape sequence
    if (escape_next) {
        unescaped_byte = byte ^ 0xFF;
        escape_next = false;
    } else if (byte == COMM_ESCAPE_BYTE) {
        escape_next = true;
        return;
    } else {
        unescaped_byte = byte;
    }
    
    // Process based on current state
    switch (rx_state) {
        case PROTO_STATE_WAIT_SYNC:
            if (byte == COMM_SYNC_BYTE) {
                // Found sync byte, move to next state
                rx_state = PROTO_STATE_TYPE;
                rx_data_count = 0;
            }
            break;
            
        case PROTO_STATE_TYPE:
            rx_packet.type = unescaped_byte;
            rx_state = PROTO_STATE_LENGTH;
            break;
            
        case PROTO_STATE_LENGTH:
            rx_packet.length = unescaped_byte;
            rx_packet_length = unescaped_byte;
            rx_state = PROTO_STATE_SEQUENCE;
            break;
            
        case PROTO_STATE_SEQUENCE:
            rx_packet.sequence = unescaped_byte;
            
            if (rx_packet_length > 0) {
                rx_state = PROTO_STATE_DATA;
            } else {
                rx_state = PROTO_STATE_CRC_HIGH;
            }
            break;
            
        case PROTO_STATE_DATA:
            if (rx_data_count < COMM_MAX_PACKET_SIZE) {
                rx_packet.data[rx_data_count++] = unescaped_byte;
            }
            
            if (rx_data_count >= rx_packet_length) {
                rx_state = PROTO_STATE_CRC_HIGH;
            }
            break;
            
        case PROTO_STATE_CRC_HIGH:
            rx_packet.crc = unescaped_byte << 8;
            rx_state = PROTO_STATE_CRC_LOW;
            break;
            
        case PROTO_STATE_CRC_LOW:
            rx_packet.crc |= unescaped_byte;
            
            // Verify CRC
            uint8_t header[3] = {rx_packet.type, rx_packet.length, rx_packet.sequence};
            uint16_t calculated_crc = comm_calculate_crc(header, 3);
            calculated_crc = comm_calculate_crc_continue(rx_packet.data, rx_packet.length, calculated_crc);
            
            if (calculated_crc == rx_packet.crc) {
                // Process valid packet
                comm_process_command(&rx_packet);
            } else {
                // Send NACK for invalid CRC
                comm_send_nack(rx_packet.sequence, ERR_CRC_FAILURE);
            }
            
            // Reset state machine
            rx_state = PROTO_STATE_WAIT_SYNC;
            break;
    }
}

/* UART Interrupt Handlers */

/**
 * UART RX Complete interrupt handler
 */
ISR(USART_RX_vect) {
    uint8_t data = UDR0;
    
    // Process received byte
    process_rx_byte(data);
    
    // Also store to RX buffer for debugging or additional processing
    ringbuffer_push(&uart_rx_buffer, data);
}

/**
 * UART Data Register Empty interrupt handler
 */
ISR(USART_UDRE_vect) {
    uint8_t data;
    
    if (ringbuffer_pop(&uart_tx_buffer, &data)) {
        // Send next byte from buffer
        UDR0 = data;
    } else {
        // Buffer is empty, disable interrupt
        UCSR0B &= ~(1 << UDRIE0);
    }
} 