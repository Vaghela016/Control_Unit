SHELL := cmd.exe

# ==================================================
# DYNAMIC CHIP SELECTION
# Run `make` for default ATmega328P
# Run `make CHIP=pb` for ATmega328PB
# ==================================================

CHIP ?= p

ifeq ($(CHIP), pb)
    MCU = atmega328pb
    PROGRAMMER_MCU = m328pb

    # ATmega328PB standalone internal 8 MHz fuse setting
    # lfuse 0xE2 = internal 8 MHz RC oscillator, CKDIV8 disabled
    # hfuse 0xDF = reset enabled, SPI programming enabled, start from application
    # efuse 0xFD = brown-out detection around 2.7 V
    LFUSE = 0xE2
    HFUSE = 0xDF
    EFUSE = 0xFD

else ifeq ($(CHIP), p)
    MCU = atmega328p
    PROGRAMMER_MCU = m328p

    # ATmega328P standalone internal 8 MHz fuse setting
    # lfuse 0xE2 = internal 8 MHz RC oscillator, CKDIV8 disabled
    # hfuse 0xDF = reset enabled, SPI programming enabled, start from application
    # efuse 0xFD = brown-out detection around 2.7 V
    LFUSE = 0xE2
    HFUSE = 0xDF
    EFUSE = 0xFD

else
    $(error Invalid CHIP specified. Use "make CHIP=p" or "make CHIP=pb")
endif

# --------------------------------------------------
# Bedienteil_test
# --------------------------------------------------

TARGET = Bedienteil_test

# Must match fuse clock.
# With lfuse = 0xE2, the real chip clock is internal 8 MHz.
F_CPU = 8000000UL

# --------------------------------------------------
# Toolchain path
# --------------------------------------------------

TOOLCHAIN = C:\avr8-gnu-toolchain-win32_x86_64\bin

CC      = "$(TOOLCHAIN)/avr-gcc.exe"
OBJCOPY = "$(TOOLCHAIN)/avr-objcopy.exe"
SIZE    = "$(TOOLCHAIN)/avr-size.exe"

AVRDUDE      = "C:\Users\namya\.platformio\packages\tool-avrdude\avrdude.exe"
AVRDUDE_CONF = "C:\Users\namya\.platformio\packages\tool-avrdude\avrdude.conf"

# --------------------------------------------------
# LCD settings
# --------------------------------------------------

LCD_ADDR = 0x3C
LCD_TWI_HZ = 100000L

# --------------------------------------------------
# Programmer settings
# --------------------------------------------------

PORT = COM9
PROGRAMMER = stk500v2

# Optional slow ISP clock.
# Useful for brand-new chips that may still run at 1 MHz because CKDIV8 is enabled.
# Example:
# make fuse CHIP=p BITCLOCK=125kHz
BITCLOCK ?=

ifeq ($(BITCLOCK),)
    AVRDUDE_BITCLOCK =
else
    AVRDUDE_BITCLOCK = -B $(BITCLOCK)
endif

AVRDUDE_FLAGS = -C $(AVRDUDE_CONF) -P $(PORT) -p $(PROGRAMMER_MCU) -c $(PROGRAMMER) $(AVRDUDE_BITCLOCK)

# --------------------------------------------------
# Source files
# --------------------------------------------------

SRC = Bedienteil_test.c \
      lcd_mccog42005a6w.c \
      i2cmaster.c

OBJ = Bedienteil_test.o \
      lcd_mccog42005a6w.o \
      i2cmaster.o

# --------------------------------------------------
# Fuse settings
# --------------------------------------------------

FUSES = -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m

# --------------------------------------------------
# Compiler flags
# --------------------------------------------------

CFLAGS = -mmcu=$(MCU) \
         -DF_CPU=$(F_CPU) \
         -DLCD_I2C_ADDR=$(LCD_ADDR) \
         -DLCD_TWI_HZ=$(LCD_TWI_HZ) \
         -std=c99 \
         -Os \
         -Wall \
         -Wextra \
         -I.

LDFLAGS = -mmcu=$(MCU)

# --------------------------------------------------
# Build rules
# --------------------------------------------------

.PHONY: all clean program flash size rebuild fuse read-fuses setup

all: $(TARGET).hex

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $(TARGET).elf $(TARGET).hex
	$(SIZE) -B $(TARGET).elf

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) -o $(TARGET).elf $(OBJ)

Bedienteil_test.o: Bedienteil_test.c lcd_mccog42005a6w.h
	$(CC) $(CFLAGS) -c Bedienteil_test.c -o Bedienteil_test.o

lcd_mccog42005a6w.o: lcd_mccog42005a6w.c lcd_mccog42005a6w.h
	$(CC) $(CFLAGS) -c lcd_mccog42005a6w.c -o lcd_mccog42005a6w.o

i2cmaster.o: i2cmaster.c i2cmaster.h
    $(CC) $(CFLAGS) -c i2cmaster.c -o i2cmaster.o

# --------------------------------------------------
# Upload to real ATmega
# --------------------------------------------------

program: $(TARGET).hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(TARGET).hex:i

flash: program

# --------------------------------------------------
# Fuse commands
# --------------------------------------------------

# Write fuses for internal 8 MHz standalone operation.
# Use this once for a new/replaced chip.
fuse:
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(FUSES)

# Read current fuse values.
# Expected for internal 8 MHz standalone:
# lfuse = 0xe2
# hfuse = 0xdf
# efuse = 0xfd
read-fuses:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

# Full setup for a new chip:
# 1. Write internal 8 MHz fuses
# 2. Flash application
setup: fuse program

# --------------------------------------------------
# Show memory size & Clean
# --------------------------------------------------

size: $(TARGET).elf
	$(SIZE) -B $(TARGET).elf

rebuild: clean all

clean:
	-del /f /q $(TARGET).hex $(TARGET).elf *.o