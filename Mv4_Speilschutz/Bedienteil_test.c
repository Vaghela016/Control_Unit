#ifndef F_CPU
#define F_CPU 8000000UL // 8 MHz standalone clock as specified in your Makefile
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
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

    // initialize timer for motor control and cooldown tracking
    timer_setup();
    sei(); // Enable global interrupts for timer-based tasks

    // Initialize LCD Display
    lcdBegin();

    // Initialize Motor Control
    motor_init();
    volatile uint16_t speed;

    // Initialize ADC for Voltage reading
    adc_init();
    

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

            voltage_check(); // Check voltage before processing commands

            // Status LED: Keep PD7 steady ON
            PORTD |= (1 << PD7);

            if (adc_read(0) < 360) { // If voltage is below 0.66V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("LOW VOLTAGE");
                    lcdSetCursor(2, 1);
                    lcdWriteText("MOTOR STOPPED");
                    return 0; // Stop the program to prevent damage from low voltage
            }
            else if (adc_read(0) > 650) { // If voltage is above 3.1V (assuming 3.3V reference, accounting for noise)
                    lcdSetCursor(1, 1);
                    lcdWriteText("OVER VOLTAGE");
                    lcdSetCursor(2, 1);
                    lcdWriteText("MOTOR STOPPED");
                    return 0; // Stop the program to prevent damage from overvoltage
            }
            else{

            // -- Evaluate Switches from Byte 1 --
            
            if (byte1 & (1 << 1)) {
                motor_set(M1_FWD, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 1 OBEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 1 FWD");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY REV ALLOWED");
                }
                   
            } 
            
            // Motor 1 reverse controlled by Schalter 1 unten (Byte 1, Bit 0)
            else if (byte1 & (1 << 0)) {
                motor_set(M1_REV, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 1 UNTEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 1 REV");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY REV ALLOWED");
                }
                 
            }

            else if (byte1 & (1 << 3)) {
                motor_set(M2_FWD, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 2 OBEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 2 FWD");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY REV ALLOWED");
                }
                 
            }
            
            else if (byte1 & (1 << 2)) {
                motor_set(M2_REV, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 2 UNTEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 2 REV");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY FWD ALLOWED");
                }
                
            }

            else if (byte1 & (1 << 5)) {
                motor_set(M3_FWD, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 3 OBEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 3 FWD");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY REV ALLOWED");
                }
                
            }

            else if (byte1 & (1 << 4)) {
                motor_set(M3_REV, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 3 UNTEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 3 REV");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY FWD ALLOWED");
                }
                
                
            }

            else if (byte1 & (1 << 7)) {
                motor_set(M4_FWD, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 4 OBEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 4 FWD");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY REV ALLOWED");
                }
                
            }

            else if (byte1 & (1 << 6)) {
                motor_set(M4_REV, speed);
                lcdSetCursor(1, 1);
                lcdWriteText("Schalter 4 UNTEN");
                lcdSetCursor(2, 1);
                lcdWriteText("Motor 4 REV");

                // Cooldown Protection
                if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                    lcdSetCursor(1, 1);
                    lcdWriteText("MOTOR COOLING DOWN");
                }

                // Voltage Protection
                
                if (adc_read(1) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                    lcdSetCursor(1, 1);
                    lcdWriteText("END POSITION");
                    lcdSetCursor(2, 1);
                    lcdWriteText("ONLY FWD ALLOWED");
                }
                
                
            }
            else {
                motor_set(M_STOP, 0);
            }
        }

        } 
        
        else if (ret != 0) {
            // NACK RECEIVED: Device failed to respond
            
            // Release the bus
            i2c_stop();

            // Status LED: Toggle PD7 to create a blinking effect
            PORTD ^= (1 << PD7);
            // Display error message on LCD
            lcdSetCursor(1, 7);
            lcdWriteText("NACK");
        }
        // Polling delay to prevent excessive bus traffic and allow for human interaction
        _delay_ms(50);
    }
    
    return 0;
}