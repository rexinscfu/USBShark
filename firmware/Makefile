# USBShark Firmware Makefile - Military-grade USB protocol analyzer
# For AVR ATmega microcontrollers (Arduino compatible)

# MCU settings
MCU = atmega328p
F_CPU = 16000000UL
BAUD = 115200

# Compiler and linker flags
CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SIZE = avr-size
AVRDUDE = avrdude

# Programmer settings (for Arduino Uno)
PROGRAMMER = arduino
PORT = /dev/ttyACM0

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
BINDIR = $(BUILDDIR)/bin

# Source and object files
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC))
HEADERS = $(wildcard $(INCDIR)/*.h)

# Target
TARGET = $(BINDIR)/usbshark

# Flags
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBAUD=$(BAUD) -Os -Wall -Wextra -std=gnu99 -ffunction-sections -fdata-sections
CFLAGS += -I$(INCDIR)
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections

# Rules
.PHONY: all clean flash dump size

all: $(TARGET).hex size

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	@mkdir -p $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(TARGET).elf
	$(SIZE) --format=avr --mcu=$(MCU) $<

flash: $(TARGET).hex
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -U flash:w:$<:i

dump: $(TARGET).elf
	$(OBJDUMP) -d $< > $(TARGET).lst

clean:
	rm -rf $(BUILDDIR)/* 