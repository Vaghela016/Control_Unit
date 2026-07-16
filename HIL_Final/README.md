# Automotive Memory Seat Control Unit (Sitzbedienteil)

## Overview
This project contains the bare-metal C firmware for an Automotive Memory Seat Control Unit, built around the **ATmega328PB** microcontroller (8 MHz). It operates as a non-blocking, interrupt-driven state machine that handles user inputs via I2C, drives multiple DC motors using PWM, tracks seat position kinematically, and ensures strict hardware safety through continuous analog current monitoring.

## Key Features
* **Virtual Kinematic Positioning:** Tracks seat position using time-and-PWM-based mathematical modeling, saving coordinates to non-volatile EEPROM memory to survive power loss.
* **Welcome Feature (Auto-Calibration):** Upon boot, the seat automatically drives to the physical mechanical end-stop to zero its coordinate system, then returns to its exact pre-power-off position.
* **Non-Blocking Memory UI:** Users can save (hold 2s), delete (hold 5s), or recall seat positions with instant LCD UI feedback, without halting the main control loop.
* **Automotive Fault Detection (VNH7070):** Continuously monitors the motor driver's Current Sense (CS) pin to instantly halt operation upon detecting:
  * **Stall Current / End-Position:** Mechanical limits hit.
  * **Open Load:** Wire breaks or disconnected motors.
* **Inrush Current Blanking:** Intelligently ignores the massive electrical current spikes that occur during motor startup and speed shifts to prevent false fault triggers.
* **Power Management:** Automatically enters low-power Sleep Mode ("Nap Time") when idle.

---

## Software Architecture
The firmware is highly modular to separate the UI layer, hardware drivers, and mathematical logic. 

### Core Modules
* **`Bedienteil_test.c` (Main Application):** The master 50ms (20Hz) State Machine. It decodes I2C switch inputs, coordinates the UI display, manages the sleep timers, and executes the safety fault shields.
* **`seat_pos.c` / `seat_pos.h` (Memory & Kinematics):** A "black box" library that handles all memory seat logic. It tracks virtual position, manages EEPROM wear-leveling, runs the Welcome calibration sequence, and handles the 2s/5s button timers.
* **`M1_test.c` / `M1_test.h` (Hardware Abstraction Layer):** Contains the direct hardware drivers for the DC motors (Timer1 PWM generation) and the Analog-to-Digital Converter (ADC) for current and voltage sensing.

### Peripheral Drivers
* **`i2cmaster.c` / `i2cmaster.h`:** Hardware TWI/I2C master library for reading the seat switch matrices.
* **`lcd_mccog42005a6w.c` / `lcd_mccog42005a6w.h`:** I2C display driver for the local user interface.

---

## Hardware Integration
* **MCU:** ATmega328PB (Internal 8MHz oscillator)
* **Motor Driver:** VNH7070ASTR
* **Input:** I2C-based switch matrix (Address: `0x40`)
* **Display:** I2C Character LCD (Address: `0x3C`)

---

## Build Instructions
This project uses a standard `makefile` configured for the `avr-gcc` toolchain.

1. Ensure you have `avr-gcc`, `avr-libc`, and `avrdude` installed.
2. Open a terminal in the project directory.
3. Compile the firmware:
   make all
4. fuse MCU to 8 MHz:
    make fuse
5. flash the firmware:
    make program
6. Delete .obj/.hex files:
    make clean


## Development Note
AI-supported development assistance for structured analysis of build logs, debugging steps and documentation, technical validation was performed via compiler outputs, tests and hardware measurements.