# USBShark Hardware Setup

This document provides detailed instructions for setting up the hardware components of the USBShark USB protocol analyzer.

## Required Components

1. Arduino Uno or compatible AVR-based board
2. USB Type-A female connector (for monitoring USB devices)
3. USB Type-A male connector or cable (for connecting monitored devices)
4. 4x LEDs: Power (Green), Activity (Blue), USB (Yellow), Error (Red)
5. 4x 220Ω resistors (for LEDs)
6. 2x 68Ω resistors (for USB data lines)
7. 2x 1.5kΩ resistors (for USB data lines)
8. Breadboard and jumper wires
9. USB cable for Arduino programming

## Wiring Diagram

```
                   Arduino Uno
                 +------------+
                 |            |
USB 5V -----+----| PC0 (ADC0) |
            |    |            |
USB D+ -----+----| PD2 (INT0) |
            |    |            |
USB D- -----+----| PD3        |
            |    |            |
GND ---------+---| GND        |
                 |            |
POWER LED -------| PB0        |
                 |            |
ACTIVITY LED ----| PB1        |
                 |            |
USB LED ----------| PB2        |
                 |            |
ERROR LED --------| PB3        |
                 |            |
                 +------------+
```

## USB Monitoring Circuit

To safely monitor USB traffic, the hardware needs to be connected as follows:

### USB Connectors

1. **USB Type-A Female Connector** (Host side):
   - Pin 1 (VBUS): Connect to Arduino PC0 through a voltage divider (10kΩ and 10kΩ) for 5V sensing
   - Pin 2 (D-): Connect to Arduino PD3 through a 68Ω resistor
   - Pin 3 (D+): Connect to Arduino PD2 (INT0) through a 68Ω resistor
   - Pin 4 (GND): Connect to Arduino GND

2. **USB Type-A Male Connector** (Device side):
   - Pin 1 (VBUS): Connect directly to USB Female Pin 1
   - Pin 2 (D-): Connect directly to USB Female Pin 2
   - Pin 3 (D+): Connect directly to USB Female Pin 3
   - Pin 4 (GND): Connect directly to USB Female Pin 4

### Pull-up/Pull-down Resistors

For proper USB bus state detection:
- Add a 1.5kΩ pull-up resistor from D+ to VBUS (full-speed device detection)
- Add a 1.5kΩ pull-down resistor from D- to GND (prevents floating)

### Status LEDs

Connect LEDs with 220Ω current-limiting resistors to Arduino pins:
- **Power LED** (Green): Arduino PB0 to LED anode, LED cathode to GND through 220Ω resistor
- **Activity LED** (Blue): Arduino PB1 to LED anode, LED cathode to GND through 220Ω resistor
- **USB LED** (Yellow): Arduino PB2 to LED anode, LED cathode to GND through 220Ω resistor
- **Error LED** (Red): Arduino PB3 to LED anode, LED cathode to GND through 220Ω resistor

## Important Notes

1. **Do not** connect the circuit to high-voltage or high-current USB devices.
2. **Always** connect the Arduino to the computer before connecting any USB devices to the monitoring circuit.
3. The circuit is designed for monitoring USB 1.x and 2.0 low/full-speed devices. High-speed devices may require additional hardware.
4. This setup creates a direct connection between the host and device, with the Arduino monitoring the signals without interfering.

## Operational Indicators

- **Power LED** (Green): Always on when the Arduino is powered
- **Activity LED** (Blue): Blinks when USB traffic is detected
- **USB LED** (Yellow): On when a USB device is connected and detected
- **Error LED** (Red): On when an error occurs, blinks in patterns to indicate error code

## USB Monitoring Limitations

The current hardware setup has the following limitations:
- Maximum monitoring speed: 12 Mbps (USB Full Speed)
- Maximum cable length: 2 meters total (host to Arduino + Arduino to device)
- No power delivery monitoring or control
- No USB 3.0/3.1 support

For high-speed (480 Mbps) monitoring, additional hardware like a dedicated USB PHY transceiver is required. 