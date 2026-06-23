#ifndef F_CPU
#define F_CPU 8000000UL // 8 MHz standalone clock as specified in your Makefile
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "i2cmaster.h"
#include "lcd_mccog42005a6w.h"

// Bedienteil 7-bit I2C address is 0b0100000 (0x20)
#define BEDIENTEIL_ADDR 0x40

// Shift address by 1 and combine with the Read bit from Peter Fleury's library
#define BEDIENTEIL_READ ((BEDIENTEIL_ADDR << 1) | I2C_READ)

void setup() {
    // Configure PD5, PD6, and PD7 as outputs for LEDs
    DDRD |= (1 << PD5) | (1 << PD6) | (1 << PD7);
    DDRB |= (1 << PB5);
    
    // Initialize all LEDs to OFF
    PORTD &= ~((1 << PD5) | (1 << PD6) | (1 << PD7));
    PORTB &= ~(1 << PB5);
}

int main(void) {
    
    // Initialize I2C Master
    i2c_init();
    setup();

    // Initialize LCD Display
    lcdBegin();


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
            
            // PD5 controlled by Schalter 1 oben (Byte 1, Bit 1)
            if (byte1 & (1 << 1)) {
                PORTD |= (1 << PD5);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 1 OBEN");
            } 
            else {
                PORTD &= ~(1 << PD5);
            }

            // PD6 controlled by Schalter 2 oben (Byte 1, Bit 3)
            if (byte1 & (1 << 0)) {
                PORTD |= (1 << PD6);
                lcdClear();
                lcdSetCursor(0, 0);
                lcdWriteText("Schalter 1 UNTEN");
            } 
            else{
                PORTD &= ~(1 << PD6);
            }

        } 
        
        else if (ret != 0) {
            // NACK RECEIVED: Device failed to respond
            
            // Release the bus
            i2c_stop();

            // Status LED: Toggle PD7 to create a blinking effect
            PORTD ^= (1 << PD7);

            // Safety measure: Turn off the switch-controlled LEDs when disconnected
            PORTD &= ~((1 << PD5) | (1 << PD6));
        }
        // Polling delay to prevent excessive bus traffic and allow for human interaction
        _delay_ms(250);
    }
    
    return 0;
}