#ifndef F_CPU
#define F_CPU 8000000UL // 8 MHz standalone clock as specified in your Makefile
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "i2cmaster.h"
#include "lcd_mccog42005a6w.h"
#include "M1_test.h"

// Bedienteil 7-bit I2C address is 0b0100000 (0x20)
#define BEDIENTEIL_ADDR 0x40

// Shift address by 1 and combine with the Read bit from Peter Fleury's library
#define BEDIENTEIL_READ ((BEDIENTEIL_ADDR << 1) | I2C_READ)


void setup() {
    // Configure PD5, PD6, and PD7 as outputs for LEDs
    DDRD &= ~(1 << PD7); // PD7 as input for Hall sensor (active HIGH)
    DDRB |= (1 << PB0);
    
    // Initialize all LEDs to OFF
    PORTD &= ~(1 << PD7);
    PORTB |= (1 << PB0);
}

int main(void) {

    // Clear reset flags and disable watchdog timer
    MCUSR = 0; 
    wdt_disable();

    // Initialize I2C Master
    i2c_init();
    setup();

    // initialize timer for motor control and cooldown tracking
    timer_setup();
    sei(); // Enable global interrupts for timer-based tasks

    // Initialize LCD Display
    lcdBegin();
    volatile uint8_t current_state = 0; // To track the current state of the motor for display purposes
    volatile uint8_t last_state = 0; // To track the last state of the motor for display purposes
    
    // Initialize Motor Control
    motor_init();
    volatile uint16_t speed;

    // Initialize ADC for Voltage reading
    adc_init();

    // Rotation counter for Hall sensor
    uint16_t hall_value = 0; // counter value of the Hall sensor
    uint8_t hall_Cstate = 0;
    uint8_t hall_Lstate = 0;

    // Enable watchdog timer with a 1-second timeout to reset in case of a hang
    if (!(adc_read(0) < 360 || adc_read(0) > 650)) { // If voltage is out of safe range at startup
        wdt_enable(WDTO_250MS);
    }
    

    //  Main Loop
    while (1) {

        // Attempt to establish a read connection with the Bedienteil
        unsigned char ret = i2c_start(BEDIENTEIL_READ);
        /*
        // Initial Voltage Check at Startup
        voltage_check(); // Check voltage before processing commands
        if (adc_read(0) < 360) { // If voltage is below 0.66V (assuming 3.3V reference)
            lcdSetCursor(1, 1);
            lcdWriteText("E_07:");
            lcdSetCursor(2, 1);
            lcdWriteText("U_KL30 < 09");
            return 0; // Stop the program to prevent damage from low voltage
        }
        else if (adc_read(0) > 650) { // If voltage is above 3.1V (assuming 3.3V reference, accounting for noise)
            lcdSetCursor(1, 1);
            lcdWriteText("E_06:");
            lcdSetCursor(2, 1);
            lcdWriteText("U_KL30 > 16");
            return 0; // Stop the program to prevent damage from overvoltage
        }
            */

        if (ret == 0) {
            // ACK RECEIVED: Device is successfully addressed
            wdt_reset(); // Reset watchdog timer to prevent reset during normal operation
            // Read the 3 bytes as per the specification: ACK, ACK, NACK
            uint8_t byte1 = i2c_readAck();
            uint8_t byte2 = i2c_readAck();
            uint8_t byte3 = i2c_readNak(); 
            
            // Release the I2C bus
            i2c_stop();

            hall_Cstate = (PIND & (1 << PD7)) ? 1 : 0; 

            if (hall_Cstate != hall_Lstate) { // Check if Hall sensor state has changed){
                if (hall_Cstate == 1) { // Check if the Hall sensor is active (active HIGH on PD7)
                    hall_value++; // Increment the Hall sensor counter
                    lcdSetCursor(3, 1);
                    lcdWriteTextf("Hall Sensor: %u ", hall_value-1);
                    hall_Cstate = 1; // Set state to indicate we've registered the Hall sensor activation
                }
                hall_Lstate = hall_Cstate; // Update last state for next comparison
            }


                if (current_state != last_state) { // Only update display if state has changed or if it's the initial state
                    lcdClear();
                }
                
                // If the state hasn't changed, we can skip updating the display to reduce flicker
                  
                    // -- Evaluate Switches from Byte 1 --
            
                    if (byte1 & (1 << 1)) {
                        current_state = 3; // Update current state for display purposes
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        motor_set(M1_FWD, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 1 OBEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 1 FWD");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }

                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY REV ALLOWED");
                        }
                   
                    } 
            
                    // Motor 1 reverse controlled by Schalter 1 unten (Byte 1, Bit 0)
                    else if (byte1 & (1 << 0)) {
                        current_state = 4; // Update current state for display purposes
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        motor_set(M1_REV, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 1 UNTEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 1 REV");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }

                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY REV ALLOWED");
                        }
                 
                    }

                    else if (byte1 & (1 << 3)) {
                        current_state = 5; // Update current state for display purposes
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        motor_set(M2_FWD, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 2 OBEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 2 FWD");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }
                

                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY REV ALLOWED");
                        }
                 
                    }
            
                    else if (byte1 & (1 << 2)) {
                        current_state = 6; // Update current state for display purposes
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        motor_set(M2_REV, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 2 UNTEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 2 REV");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }
                

                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY FWD ALLOWED");
                        }
                
                    }

                    else if (byte1 & (1 << 5)) {
                        current_state = 7; // Update current state for display purposes
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        motor_set(M3_FWD, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 3 OBEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 3 FWD");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }

                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY REV ALLOWED");
                        }
                    
                    }

                    else if (byte1 & (1 << 4)) {
                        current_state = 8; // Update current state for display purposes
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        motor_set(M3_REV, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 3 UNTEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 3 REV");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }
                
                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY FWD ALLOWED");
                        }
                
                    }

                    else if (byte1 & (1 << 7)) {
                        current_state = 9; // Update current state for display purposes
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        motor_set(M4_FWD, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 4 OBEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 4 FWD");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }
                
                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY REV ALLOWED");
                        }
                
                    }

                    else if (byte1 & (1 << 6)) {
                        speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                        current_state = 1; // Update current state for display purposes
                        motor_set(M4_REV, speed);
                        lcdSetCursor(1, 1);
                        lcdWriteText("Schalter 4 UNTEN");
                        lcdSetCursor(2, 1);
                        lcdWriteText("Motor 4 REV");

                        // Cooldown Protection
                        if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("MOTOR COOLING DOWN");
                        }
                
                        // Voltage Protection
                        if (adc_avg(7) >= 436) { // If voltage is above 1.5V (assuming 3.3V reference)
                            lcdClear();
                            lcdSetCursor(1, 1);
                            lcdWriteText("END POSITION");
                            lcdSetCursor(2, 1);
                            lcdWriteText("ONLY FWD ALLOWED");
                        }
                
                    }
                    else {
                        motor_set(M_STOP, 0);
                    
                
                current_state = last_state; // Update last state for next iteration
            }

        } 
        
        else if (ret != 0) {
            // NACK RECEIVED: Device failed to respond
            // Display error message on LCD
            lcdClear();
            lcdSetCursor(1, 7);
            lcdWriteText("NACK");
            
            // Release the bus
            i2c_stop();

            // Status LED: Toggle PD7 to create a blinking effect
            PORTD ^= (1 << PD7);
            
        }
        // Polling delay to prevent excessive bus traffic and allow for human interaction
        _delay_ms(100);
    }
    
    return 0;
}