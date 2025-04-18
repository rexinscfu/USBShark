/**
 * USBShark - Military-grade USB protocol analyzer
 * Communication Protocol Header - Defines the binary protocol between Arduino and host
 */

#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* Protocol constants */
#define COMM_SYNC_BYTE       0xAA
#define COMM_ESCAPE_BYTE     0x55
#define COMM_MAX_PACKET_SIZE 255
#define COMM_HEADER_SIZE     4
#define COMM_FOOTER_SIZE     2

/* Packet types */
typedef enum {
    /* Control messages (host to device) */
    PACKET_TYPE_CMD_RESET         = 0x01,
    PACKET_TYPE_CMD_START_CAPTURE = 0x02,
    PACKET_TYPE_CMD_STOP_CAPTURE  = 0x03,
    PACKET_TYPE_CMD_SET_FILTER    = 0x04,
    PACKET_TYPE_CMD_GET_STATUS    = 0x05,
    PACKET_TYPE_CMD_SET_TIMESTAMP = 0x06,
    PACKET_TYPE_CMD_SET_CONFIG    = 0x07,
    
    /* Data messages (device to host) */
    PACKET_TYPE_USB_PACKET        = 0x80,
    PACKET_TYPE_USB_STATE_CHANGE  = 0x81,
    PACKET_TYPE_STATUS_REPORT     = 0x82,
    PACKET_TYPE_ERROR_REPORT      = 0x83,
    PACKET_TYPE_BUFFER_OVERFLOW   = 0x84,
    PACKET_TYPE_DEV_DESCRIPTOR    = 0x85,
    PACKET_TYPE_CONFIG_DESCRIPTOR = 0x86,
    PACKET_TYPE_STRING_DESCRIPTOR = 0x87,
    
    /* Acknowledgments */
    PACKET_TYPE_ACK               = 0xF0,
    PACKET_TYPE_NACK              = 0xF1
} packet_type_t;

/* Error codes */
typedef enum {
    ERR_NONE            = 0x00,
    ERR_INVALID_COMMAND = 0x01,
    ERR_BUFFER_OVERFLOW = 0x02,
    ERR_CRC_FAILURE     = 0x03,
    ERR_INVALID_STATE   = 0x04,
    ERR_USB_ERROR       = 0x05,
    ERR_TIMEOUT         = 0x06,
    ERR_INTERNAL        = 0xFF
} error_code_t;

/* Packet structure */
typedef struct {
    uint8_t sync;         // Always COMM_SYNC_BYTE
    uint8_t type;         // packet_type_t value
    uint8_t length;       // Length of data field
    uint8_t sequence;     // Packet sequence number
    uint8_t data[COMM_MAX_PACKET_SIZE];  // Packet payload
    uint16_t crc;         // CRC-16 of type, length, sequence and data
} comm_packet_t;

/* Protocol functions */
void comm_init(void);
bool comm_send_packet(packet_type_t type, const uint8_t *data, uint8_t length);
bool comm_receive_packet(comm_packet_t *packet);
bool comm_process_command(const comm_packet_t *packet);
void comm_send_ack(uint8_t sequence);
void comm_send_nack(uint8_t sequence, error_code_t error);
uint16_t comm_calculate_crc(const uint8_t *data, uint16_t length);
uint16_t comm_calculate_crc_continue(const uint8_t *data, uint16_t length, uint16_t crc);

/* Utility functions */
void comm_escape_data(uint8_t *dest, const uint8_t *src, uint8_t length, uint8_t *escaped_length);
bool comm_unescape_data(uint8_t *dest, const uint8_t *src, uint8_t length, uint8_t *unescaped_length);

/* High-level communication functions */
bool comm_send_usb_packet(const uint8_t *data, uint8_t length, uint32_t timestamp, uint8_t pid);
void comm_send_status_report(uint8_t device_count, uint8_t capture_state, uint16_t buffer_usage);
void comm_send_error(error_code_t error_code, uint8_t context);

#endif /* COMM_PROTOCOL_H */ 