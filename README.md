# USBShark

USBShark: Military-grade Arduino-powered USB protocol analyzer with real-time monitoring, packet decoding, and advanced filtering.

## Features

- **Non-intrusive USB monitoring**: Monitor USB communication without affecting data flow
- **Hardware-level packet capture**: Direct pin monitoring with precise timing
- **Support for multiple USB speeds**: Low-speed (1.5 Mbps) and Full-speed (12 Mbps) USB
- **Real-time packet analysis**: View USB traffic as it happens
- **Advanced filtering**: Filter by device address, endpoint, transfer type, and more
- **Efficient binary protocol**: High-speed data transfer between device and host
- **Cross-platform support**: Windows, macOS, and Linux desktop applications (in development)

## Hardware Requirements

- Arduino Uno or compatible AVR-based board
- USB host shield or custom USB monitoring hardware
- 4 status LEDs (power, activity, USB, error)

## Firmware Architecture

The firmware is written in pure C for maximum efficiency and direct hardware control. Key components include:

- **USB Interface**: Direct pin-level monitoring of USB D+/D- lines
- **Communication Protocol**: Efficient binary protocol for data transfer
- **Ringbuffer**: Lock-free ring buffer for high-speed data handling
- **State Machine**: Robust state management for USB monitoring

## Building the Firmware

### Prerequisites

- AVR-GCC compiler toolchain
- AVRDUDE for uploading to the microcontroller
- GNU Make

### Build Instructions

1. Clone the repository:
```
git clone https://github.com/rexinscfu/USBShark.git
cd USBShark
```

2. Build the firmware:
```
cd firmware
make
```

3. Upload to Arduino:
```
make flash
```

## USB Monitoring 

USBShark can detect and monitor USB devices by directly interfacing with the USB data lines:

- **USB D+ line**: Connected to Arduino pin PD2 (INT0)
- **USB D- line**: Connected to Arduino pin PD3
- **USB 5V sensing**: Connected to Arduino pin PC0 (ADC0)

The firmware uses direct port manipulation and interrupt-driven capture to achieve high-speed monitoring with minimal overhead.

## Communication Protocol

The device communicates with the host computer using a custom binary protocol:

- **Packet framing**: Start byte, type, length, sequence number, data, and CRC-16
- **Error detection**: CRC-16 calculation and validation
- **Flow control**: Acknowledgment packets for reliable transfer
- **Escape sequences**: Special byte handling to maintain protocol integrity

## Desktop Application

The desktop application for viewing and analyzing USB traffic is currently in development. It will provide:

- Real-time visualization of USB traffic
- Packet inspection and decoding
- Protocol-specific analyzers
- Advanced filtering and search capabilities
- Session recording and playback

## Project Status

- [x] Initial project setup
- [x] Core USB detection functionality
- [x] Communication protocol implementation
- [x] Ringbuffer implementation
- [ ] Complete USB protocol implementation
- [ ] Protocol analyzers for common USB device classes
- [ ] Desktop application

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions to USBShark are welcome! Please feel free to submit a Pull Request.
