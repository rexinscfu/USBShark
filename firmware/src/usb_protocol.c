/**
 * USBShark - Military-grade USB protocol analyzer
 * USB Protocol Implementation
 */

#include "../include/usb_protocol.h"
#include "../include/usb_interface.h"
#include <string.h>

/* CRC-5 Table (Token packets) */
static const uint8_t crc5_table[32] = {
    0x00, 0x0E, 0x1C, 0x12, 0x11, 0x1F, 0x0D, 0x03,
    0x0B, 0x05, 0x17, 0x19, 0x1A, 0x14, 0x06, 0x08,
    0x16, 0x18, 0x0A, 0x04, 0x07, 0x09, 0x1B, 0x15,
    0x1D, 0x13, 0x01, 0x0F, 0x0C, 0x02, 0x10, 0x1E
};

/* CRC-16 Table (Data packets) */
static const uint16_t crc16_table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
    0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
    0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
    0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
    0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
    0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
    0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
    0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
    0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
    0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
    0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
    0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
    0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
    0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
    0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
    0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
    0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
    0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
    0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
    0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
    0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
    0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
    0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
    0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

/**
 * Initialize the USB protocol module
 */
void usb_protocol_init(void) {
    // No specific initialization needed at this time
}

/**
 * Extract the PID value from a raw PID byte
 * @param raw_pid Raw PID byte from USB packet
 * @return Normalized PID value
 */
uint8_t usb_get_pid_from_raw(uint8_t raw_pid) {
    return raw_pid & 0x0F;
}

/**
 * Check if the PID indicates a token packet
 * @param pid PID to check
 * @return true if token packet
 */
bool usb_is_token_packet(uint8_t pid) {
    return (pid == USB_PID_OUT || pid == USB_PID_IN || 
            pid == USB_PID_SETUP || pid == USB_PID_SOF);
}

/**
 * Check if the PID indicates a data packet
 * @param pid PID to check
 * @return true if data packet
 */
bool usb_is_data_packet(uint8_t pid) {
    return (pid == USB_PID_DATA0 || pid == USB_PID_DATA1);
}

/**
 * Check if the PID indicates a handshake packet
 * @param pid PID to check
 * @return true if handshake packet
 */
bool usb_is_handshake_packet(uint8_t pid) {
    return (pid == USB_PID_ACK || pid == USB_PID_NAK || 
            pid == USB_PID_STALL);
}

/**
 * Check if the PID indicates a special packet
 * @param pid PID to check
 * @return true if special packet
 */
bool usb_is_special_packet(uint8_t pid) {
    return (pid == USB_PID_PRE || pid == USB_PID_ERR ||
            pid == USB_PID_SPLIT || pid == USB_PID_PING ||
            pid == USB_PID_RESERVED);
}

/**
 * Extract endpoint number from token packet
 * @param token_data Token packet data (2 bytes after PID)
 * @return Endpoint number
 */
uint8_t usb_get_endpoint_from_token(const uint8_t *token_data) {
    return ((token_data[0] & 0x07) << 1) | ((token_data[1] & 0x80) >> 7);
}

/**
 * Extract device address from token packet
 * @param token_data Token packet data (2 bytes after PID)
 * @return Device address
 */
uint8_t usb_get_address_from_token(const uint8_t *token_data) {
    return (token_data[1] & 0x7F);
}

/**
 * Decode a USB packet from raw data
 * @param data Raw packet data including PID
 * @param length Length of data
 * @param packet Structure to fill with decoded packet info
 * @return true if successful
 */
bool usb_decode_packet(const uint8_t *data, uint16_t length, usb_packet_t *packet) {
    if (length < 1) {
        return false;
    }
    
    // Extract PID
    packet->pid = data[0];
    
    // Process based on packet type
    if (usb_is_token_packet(packet->pid)) {
        if (length < 3) {
            return false; // Token packets must be at least 3 bytes
        }
        
        // Extract address and endpoint
        packet->dev_addr = usb_get_address_from_token(&data[1]);
        packet->endpoint = usb_get_endpoint_from_token(&data[1]);
        
        // Validate CRC5
        uint16_t token = (data[1] | (data[2] << 8)) & 0x07FF;
        packet->crc_valid = (usb_calculate_crc5(token) == (data[2] >> 3));
        
        // No data payload
        packet->data = NULL;
        packet->data_len = 0;
    }
    else if (usb_is_data_packet(packet->pid)) {
        if (length < 3) {
            return false; // Data packets need at least 3 bytes
        }
        
        // Data packets have variable length payload plus 2 CRC bytes
        packet->data_len = length - 3; // Subtract PID and CRC16
        
        if (packet->data_len > 0) {
            packet->data = &data[1];
            
            // Validate CRC16
            uint16_t crc = usb_calculate_crc16(packet->data, packet->data_len);
            uint16_t packet_crc = (data[length-1] << 8) | data[length-2];
            packet->crc_valid = (crc == packet_crc);
        } else {
            packet->data = NULL;
            packet->crc_valid = true; // Zero-length data packets are valid
        }
    }
    else if (usb_is_handshake_packet(packet->pid)) {
        // Handshake packets have only PID, no data or CRC
        packet->data = NULL;
        packet->data_len = 0;
        packet->crc_valid = true;
        packet->dev_addr = 0;
        packet->endpoint = 0;
    }
    else {
        // Unknown packet type
        return false;
    }
    
    return true;
}

/**
 * Decode a USB setup packet from data phase
 * @param data Raw setup packet data (8 bytes)
 * @param setup Structure to fill with decoded setup info
 */
void usb_decode_setup_packet(const uint8_t *data, usb_setup_packet_t *setup) {
    setup->bmRequestType = data[0];
    setup->bRequest = data[1];
    setup->wValue = (data[3] << 8) | data[2];
    setup->wIndex = (data[5] << 8) | data[4];
    setup->wLength = (data[7] << 8) | data[6];
}

/**
 * Check if a setup packet is for a control transfer
 * @param setup Setup packet structure
 * @return true if control transfer
 */
bool usb_is_control_transfer(const usb_setup_packet_t *setup) {
    // Control transfers are indicated by specific request types
    return ((setup->bmRequestType & 0x60) == 0x00); // Standard request
}

/**
 * Calculate CRC16 for USB data packets
 * @param data Data buffer
 * @param length Length of data
 * @return Calculated CRC16
 */
uint16_t usb_calculate_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    
    return ~crc; // Final CRC is complement of calculation
}

/**
 * Calculate CRC5 for USB token packets
 * @param data 11 bits of token data
 * @return Calculated CRC5
 */
uint8_t usb_calculate_crc5(uint16_t data) {
    uint8_t crc = 0x1F;
    
    for (int i = 0; i < 11; i++) {
        if ((crc ^ data) & 0x01) {
            crc = (crc >> 1) ^ 0x14;
        } else {
            crc = crc >> 1;
        }
        data = data >> 1;
    }
    
    return crc;
} 