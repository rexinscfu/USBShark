/**
 * USBShark - Military-grade USB protocol analyzer
 * USB Interface Header
 */

#ifndef USB_INTERFACE_H
#define USB_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

/* USB Packet Types */
typedef enum {
    USB_PID_OUT   = 0xE1,
    USB_PID_IN    = 0x69,
    USB_PID_SOF   = 0xA5,
    USB_PID_SETUP = 0x2D,
    USB_PID_DATA0 = 0xC3,
    USB_PID_DATA1 = 0x4B,
    USB_PID_ACK   = 0xD2,
    USB_PID_NAK   = 0x5A,
    USB_PID_STALL = 0x1E
} usb_pid_t;

/* USB Speed */
typedef enum {
    USB_SPEED_LOW  = 0,
    USB_SPEED_FULL = 1, 
    USB_SPEED_HIGH = 2
} usb_speed_t;

/* USB Packet Structure */
typedef struct {
    uint32_t timestamp;  // Microsecond timestamp
    usb_pid_t pid;       // Packet ID
    uint8_t endpoint;    // Endpoint number
    uint8_t dev_addr;    // Device address
    uint16_t data_len;   // Length of data
    uint8_t *data;       // Pointer to data buffer
    bool crc_valid;      // CRC validity flag
} usb_packet_t;

/* USB Device State */
typedef enum {
    USB_STATE_DETACHED,
    USB_STATE_ATTACHED,
    USB_STATE_POWERED,
    USB_STATE_DEFAULT,
    USB_STATE_ADDRESS,
    USB_STATE_CONFIGURED,
    USB_STATE_SUSPENDED
} usb_state_t;

/* USB Monitoring Configuration */
typedef struct {
    usb_speed_t speed;
    bool capture_control;
    bool capture_bulk;
    bool capture_interrupt;
    bool capture_isoc;
    uint8_t addr_filter;      // 0 = no filter
    uint8_t ep_filter;        // 0 = no filter
    bool filter_in;
    bool filter_out;
} usb_monitor_config_t;

/* USB Detection and Monitoring Functions */
void usb_init(void);
bool usb_detect_bus_state(void);
void usb_monitor_enable(const usb_monitor_config_t *config);
void usb_monitor_disable(void);
uint8_t usb_get_device_count(void);

/* USB Data Capture Functions */
bool usb_capture_packet(usb_packet_t *packet);
void usb_process_packet(const usb_packet_t *packet);
void usb_send_packet_to_host(const usb_packet_t *packet);

/* Direct Hardware Interface Functions */
void usb_dp_set_input(void);
void usb_dp_set_output(void);
void usb_dm_set_input(void);
void usb_dm_set_output(void);
uint8_t usb_read_data_pins(void);

/* Utility Functions */
uint32_t usb_get_timestamp(void);
void usb_reset_timestamp(void);
bool usb_crc_check(const uint8_t *data, uint16_t length, uint16_t crc);

#endif /* USB_INTERFACE_H */ 