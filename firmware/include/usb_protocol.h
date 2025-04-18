/**
 * USBShark - Military-grade USB protocol analyzer
 * USB Protocol Details Header
 */

#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* USB Token packet bit definitions */
#define USB_TOKEN_PID_MASK    0xF0
#define USB_TOKEN_ADDR_MASK   0x7F
#define USB_TOKEN_ENDP_MASK   0x0F

/* USB Device Descriptor field offsets */
#define USB_DESC_LENGTH           0
#define USB_DESC_TYPE             1
#define USB_DESC_BCDUSB           2  // 2 bytes
#define USB_DESC_DEVICE_CLASS     4
#define USB_DESC_DEVICE_SUBCLASS  5
#define USB_DESC_DEVICE_PROTOCOL  6
#define USB_DESC_MAX_PACKET_SIZE0 7
#define USB_DESC_VENDOR_ID        8  // 2 bytes
#define USB_DESC_PRODUCT_ID       10 // 2 bytes
#define USB_DESC_BCDDEVICE        12 // 2 bytes
#define USB_DESC_MANUFACTURER     14
#define USB_DESC_PRODUCT          15
#define USB_DESC_SERIAL_NUM       16
#define USB_DESC_NUM_CONFIGS      17

/* Descriptor Types */
#define USB_DESC_TYPE_DEVICE           0x01
#define USB_DESC_TYPE_CONFIG           0x02
#define USB_DESC_TYPE_STRING           0x03
#define USB_DESC_TYPE_INTERFACE        0x04
#define USB_DESC_TYPE_ENDPOINT         0x05
#define USB_DESC_TYPE_DEVICE_QUALIFIER 0x06
#define USB_DESC_TYPE_OTHER_SPEED      0x07
#define USB_DESC_TYPE_INTERFACE_POWER  0x08
#define USB_DESC_TYPE_HID              0x21
#define USB_DESC_TYPE_REPORT           0x22
#define USB_DESC_TYPE_PHYSICAL         0x23
#define USB_DESC_TYPE_HUB              0x29

/* USB Standard Requests */
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B
#define USB_REQ_SYNCH_FRAME       0x0C

/* USB Transfer Types */
typedef enum {
    USB_TRANSFER_CONTROL = 0,
    USB_TRANSFER_ISOCHRONOUS,
    USB_TRANSFER_BULK,
    USB_TRANSFER_INTERRUPT
} usb_transfer_type_t;

/* USB Setup Packet Structure */
typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

/* USB Device Descriptor Structure */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_device_descriptor_t;

/* USB Configuration Descriptor Structure */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} usb_config_descriptor_t;

/* USB Interface Descriptor Structure */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

/* USB Endpoint Descriptor Structure */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} usb_endpoint_descriptor_t;

/* USB String Descriptor Structure */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wLANGID[1]; // Variable length array of LANGID codes
} usb_string_descriptor_t;

/* USB Protocol Functions */
void usb_protocol_init(void);
bool usb_decode_packet(const uint8_t *data, uint16_t length, usb_packet_t *packet);
uint8_t usb_get_pid_from_raw(uint8_t raw_pid);
bool usb_is_token_packet(uint8_t pid);
bool usb_is_data_packet(uint8_t pid);
bool usb_is_handshake_packet(uint8_t pid);
bool usb_is_special_packet(uint8_t pid);
uint8_t usb_get_endpoint_from_token(const uint8_t *token_data);
uint8_t usb_get_address_from_token(const uint8_t *token_data);
void usb_decode_setup_packet(const uint8_t *data, usb_setup_packet_t *setup);
bool usb_is_control_transfer(const usb_setup_packet_t *setup);
uint16_t usb_calculate_crc16(const uint8_t *data, uint16_t length);
uint8_t usb_calculate_crc5(uint16_t data);

#endif /* USB_PROTOCOL_H */ 