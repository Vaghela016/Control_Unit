#ifndef F_CPU
#define F_CPU 8000000UL // 8 MHz standalone clock as specified in your Makefile
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "i2cmaster.h"
#include "lcd_mccog42005a6w.h"
#include "M1_test.h"

// Bedienteil 7-bit I2C address is 0b0100000 (0x20)
#define BEDIENTEIL_ADDR 0x40

// Shift address by 1 and combine with the Read bit from Peter Fleury's library
#define BEDIENTEIL_READ ((BEDIENTEIL_ADDR << 1) | I2C_READ)

void setup() {
    // Configure PD5, PD6, and PD7 as outputs for LEDs
    DDRD |= (1 << PD7);
    DDRB |= (1 << PB5);
    
    // Initialize all LEDs to OFF
    PORTD &= ~(1 << PD7);
    PORTB &= ~(1 << PB5);
}

int main(void) {
    
    // Initialize I2C Master
    i2c_init();
    setup();

    // Initialize LCD Display
    lcdBegin();

    // Initialize Motor Control
    motor_init();
    volatile uint16_t speed = 200; // Set a default speed for the motor


    // 3. Main Polling Loop
    while (1) {
        // Attempt to establish a read connection with the Bedienteil
        unsigned char ret = i2c_start(BEDIENTEIL_READ);

        if (ret == 0) {
            // ACK RECEIVED: Device is successfully addressed
            
            // Read the 3 bytes as per the specification: ACK, ACK, NACK
            uint8_t byte1 = i2c_readAck();
            uint8_t byte2 = i2c_readAck();
            uint8_t byte3 = i2c_readNak(); 
            
            // Release the I2C bus
            i2c_stop();
            lcdClear();

            // Status LED: Keep PD7 steady ON
            PORTD |= (1 << PD7);

            // -- Evaluate Switches from Byte 1 --
            
            if (byte1 & (1 << 1)) {
                motor_set(M1_FWD, speed);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 1 OBEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 1 FWD");
            } 
            
            // Motor 1 reverse controlled by Schalter 1 unten (Byte 1, Bit 0)
            else if (byte1 & (1 << 0)) {
                motor_set(M1_REV, speed);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 1 UNTEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 1 REV");
            }

            else if (byte1 & (1 << 3)) {
                motor_set(M2_FWD, speed);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 2 OBEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 2 FWD");
            }
            
            else if (byte1 & (1 << 2)) {
                motor_set(M2_REV, speed);
                lcdClear();
                lcdSetCursor(0, 1);
                lcdWriteText("Schalter 2 UNTEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 2 REV");
            }

            else if (byte1 & (1 << 5)) {
                motor_set(M3_FWD, speed);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 3 OBEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 3 FWD");
            }

            else if (byte1 & (1 << 4)) {
                motor_set(M3_REV, speed);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 3 UNTEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 3 REV");
            }

            else if (byte1 & (1 << 7)) {
                motor_set(M4_FWD, speed);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 4 OBEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 4 FWD");
            }

            else if (byte1 & (1 << 6)) {
                motor_set(M4_REV, speed);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 4 UNTEN");
                lcdSetCursor(1, 0);
                lcdWriteText("Motor 4 REV");
            }
            else {
                motor_set(M_STOP, 0);
            }

        } 
        
        else if (ret != 0) {
            // NACK RECEIVED: Device failed to respond
            
            // Release the bus
            i2c_stop();

            // Status LED: Toggle PD7 to create a blinking effect
            PORTD ^= (1 << PD7);
            // Display error message on LCD
            lcdClear();
            lcdSetCursor(0, 0);
            lcdWriteText("NACK");
        }
        // Polling delay to prevent excessive bus traffic and allow for human interaction
        _delay_ms(275);
    }
    
    return 0;
}