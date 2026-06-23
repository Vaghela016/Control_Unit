#include "M1_test.h"

#ifndef F_CPU
    #define F_CPU 8000000UL // Define clock speed as 8 MHz
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "i2cmaster.h"
#include "lcd_mccog42005a6w.h"

// Cooldown parameters
volatile int16_t counters[4] = {0, 0, 0, 0}; // Assuming 4 motors
volatile uint8_t STOP_M[4] = {0, 0, 0, 0}; // To track if each motor has reached its stop threshold

// To track which motors are currently actively moving
volatile uint8_t M_Active[4] = {0, 0, 0, 0}; // To track which motors are currently active (1 for active, 0 for inactive)
volatile uint8_t M_FWD[4] = {0, 0, 0, 0};
volatile uint8_t M_REV[4] = {0, 0, 0, 0};
volatile uint8_t Wait[4] = {0, 0, 0, 0}; // To track if motor is waiting for cooldown (1 for waiting, 0 for not waiting)
volatile uint8_t MAX_PWM = 0;

// Initializes the ADC
void adc_init(void) {
    // 1. Select Reference Voltage:
    // REFS0 = 1, REFS1 = 0 sets AVCC (3.3V) as the reference.
    ADMUX = (1 << REFS0);
    
    // 2. Set Clock Prescaler and Enable ADC:
    // The ADC needs a clock between 50kHz and 200kHz to be accurate.
    // Assuming a 16MHz clock: 8,000,000 / 64 = 125kHz.
    // Set ADPS2, ADPS1, ADPS0 to 1 for a division factor of 128.
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
    
    // 3. Power Saving:
    // Disable the digital input buffer on PE3 (ADC7) to save power.
    #if defined(__AVR_ATmega328PB__)
        DIDR0 |= (1 << ADC2D); // Digital Input Disable for ADC2 (PE3 on ATmega328PB)
        DIDR0 |= (1 << ADC3D); // Digital Input Disable for ADC3 (PE4 on ATmega328PB)
    #elif defined(__AVR_ATmega328P__)
        DIDR0 |= (1 << ADC0D); // for AtMega328PB -> ADC7D
        DIDR0 |= (1 << ADC1D); // for AtMega328PB -> ADC6D
    #endif
}

// Reads an analog value from the specified channel (0-7)
uint16_t adc_read(uint8_t channel) {
    // Safety mask to ensure channel is only between 0 and 7
    channel &= 0x07; 
    
    // Clear the bottom 3 bits of ADMUX (channel selection) and set the new channel
    ADMUX = (ADMUX & 0xF8) | channel;
    
    // Start the conversion by setting the ADSC bit
    ADCSRA |= (1 << ADSC);
    
    // Wait for the conversion to complete (ADSC bit will be cleared by hardware)
    while (ADCSRA & (1 << ADSC));
    
    // Return the 10-bit ADC result (combines ADCL and ADCH automatically in avr-gcc)
    return ADC;
}

uint16_t adc_avg(uint8_t channel) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
        sum += adc_read(channel);

    }
    return sum / 4; // Return the average value
}


void motor_init(void) {
    // 1. Set pins as outputs
    DDRB |= (1 << PWM1);
    DDRD |= ((1 << M1_F) | (1 << M2_F) | (1 << M3_F) \
            | (1 << M4_F) | (1 << M4_R));
            

    // 2. Configure Timer1 for Fast PWM, Mode 14 (TOP = ICR1)
    // Clear OC1A on Compare Match, set OC1A at BOTTOM (non-inverting mode)
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    
    // Prescaler = 1 (CS10 = 1), WGM12 = 1, WGM13 = 1
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS10);

    // 3. Set the TOP value for 20 kHz
    ICR1 = PWM_MAX; // (F_CPU / (Prescaler * Frequency)) - 1 = (8,000,000 / (1 * 20,000)) - 1 = 399

    // 4. Initialize with 0 speed
    OCR1A = 0; 
}


// Hardware Setup
void timer_setup() {
    // TIMER 2: Cooldown tick (~61 Hz)
    TCCR2A = 0;  // Normal mode
    TCCR2B = 0;
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
    TIMSK2 = (1 << TOIE2); 
}


void PinChange_init(void){
    DDRD &= ~(1 << PD6); // PD6 as Input
    PORTD |= (1 << PD6); // Enable internal pull-up
}

void PinChange_setup(void){
    PCICR |= (1 << PCIE2);    // Enable Pin Change Interrupts for Port D
    PCMSK2 |= (1 << PCINT22); // Unmask PD6 specifically
}

void MCU_sleep(void) {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Configure the sleep mode depth
    sleep_enable(); // Set the Sleep Enable bit in the hardware register to unlock sleeping
    // Execute the actual assembly SLEEP instruction. 
    // The microcontroller HALTS on this exact line until PD6 gets toggled!
    sleep_cpu(); 
    sleep_disable(); // MCU just woke up! Immediately clear the Sleep Enabled bit so we don't accidentally sleep again.
}

ISR(PCINT2_vect) {
    // Empty ISR, mandatory for the wake-up to function correctly
}


// Speed should be between 0 and 399
void motor_set(uint8_t state, uint16_t speed) {
    // Cap the speed to our TOP value to prevent overflow
    /*if (speed > ICR1) {
        speed = ICR1;
    }*/
    
    

    // Set motor direction
    switch (state) {
        
        case M1_FWD:
            if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }

            PORTD |= (1 << M1_F);   //  HIGH
            PORTD &= ~((1 << M4_R) | (1 << M4_F) | (1 << M3_F) | (1 << M2_F));  // LOW
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[0] = 1; // Mark motor 1 as active
            M_FWD[0] = 1; // Mark motor 1 as moving forward
            M_REV[0] = 0; // Mark motor 1 as not moving reverse

            break;
          
        case M2_FWD:
            if (STOP_M[1] == 1) { // Check if motor 2 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }
            PORTD |= ((1 << M2_F) | (1 << M1_F));   //  HIGH
            PORTD &= ~((1 << M4_R) | (1 << M4_F) | (1 << M3_F));  // LOW
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[1] = 1; // Mark motor 2 as active
            M_FWD[1] = 1; // Mark motor 2 as moving forward
            M_REV[1] = 0; // Mark motor 2 as not moving reverse
            
            break;

        case M3_FWD:
            if (STOP_M[2] == 1) { // Check if motor 3 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }
            PORTD |= ((1 << M3_F) | (1 << M2_F) | (1 << M1_F));   //  HIGH
            PORTD &= ~((1 << M4_R) | (1 << M4_F));  // LOW
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[2] = 1; // Mark motor 3 as active
            M_FWD[2] = 1; // Mark motor 3 as moving forward
            M_REV[2] = 0; // Mark motor 3 as not moving reverse
            
            break;

        case M4_FWD:
            if (STOP_M[3] == 1) { // Check if motor 4 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }
            PORTD |= ((1 << M4_F) | (1 << M3_F) | (1 << M2_F) | (1 << M1_F));   //  HIGH
            PORTD &= ~(1 << M4_R);  // LOW
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[3] = 1; // Mark motor 4 as active
            M_FWD[3] = 1; // Mark motor 4 as moving forward
            M_REV[3] = 0; // Mark motor 4 as not moving reverse
            
            break;

        case M1_REV:
            if (STOP_M[0] == 1) { // Check if motor 1 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }
            PORTD &= ~(1 << M1_F);  // LOW
            PORTD |= ((1 << M2_F) | (1 << M3_F) | (1 << M4_F) | (1 << M4_R)); // HIGH
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[0] = 1; // Mark motor 1 as active
            M_FWD[0] = 0; // Mark motor 1 as not moving forward
            M_REV[0] = 1; // Mark motor 1 as moving reverse
            
            break;
            
        case M2_REV:
            if (STOP_M[1] == 1) { // Check if motor 2 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }
            PORTD &= ~((1 << M2_F) | (1 << M1_F));  // LOW
            PORTD |= ((1 << M4_R) | (1 << M4_F) | (1 << M3_F)); // HIGH
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[1] = 1; // Mark motor 2 as active
            M_FWD[1] = 0; // Mark motor 2 as not moving forward
            M_REV[1] = 1; // Mark motor 2 as moving reverse
            
            break;

        case M3_REV:
            if (STOP_M[2] == 1) { // Check if motor 3 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }
            PORTD &= ~((1 << M3_F) | (1 << M2_F) | (1 << M1_F));  // LOW
            PORTD |= ((1 << M4_R) | (1 << M4_F)); // HIGH
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[2] = 1; // Mark motor 3 as active
            M_FWD[2] = 0; // Mark motor 3 as not moving forward
            M_REV[2] = 1; // Mark motor 3 as moving reverse
            
            break;

        case M4_REV:
            if (STOP_M[3] == 1) { // Check if motor 4 is in cooldown
                motor_set(M_STOP, 0); // Stop the motor if it's still cooling down
                return; // Ignore command if motor is still cooling down
            }
            if (adc_avg(7) >= 436) { // If voltage is above 1.4V (assuming 3.3V reference)
                motor_set(M_STOP, 0); // Stop the motor to prevent damage
                return; // Ignore command if voltage is too high
            }
            PORTD &= ~((1 << M4_F) | (1 << M3_F) | (1 << M2_F) | (1 << M1_F));  // LOW
            PORTD |= (1 << M4_R); // HIGH
            OCR1A = speed; // Set PWM duty cycle for speed control
            M_Active[3] = 1; // Mark motor 4 as active
            M_FWD[3] = 0; // Mark motor 4 as not moving forward
            M_REV[3] = 1; // Mark motor 4 as moving reverse
            
            break;

        case M_STOP:
        default:
            PORTD &= ~((1 << M1_F) | (1 << M2_F) | (1 << M3_F) | \
                      (1 << M4_F) | (1 << M4_R));  //LOW
            M_Active[0] = 0; // Mark motor 1 as inactive
            M_Active[1] = 0; // Mark motor 2 as inactive   
            M_Active[2] = 0; // Mark motor 3 as inactive
            M_Active[3] = 0; // Mark motor 4 as inactive
            M_FWD[0] = 0; // Mark motor 1 as not moving forward
            M_REV[0] = 0; // Mark motor 1 as not moving reverse
            M_FWD[1] = 0; // Mark motor 2 as not moving forward
            M_REV[1] = 0; // Mark motor 2 as not moving reverse
            M_FWD[2] = 0; // Mark motor 3 as not moving forward
            M_REV[2] = 0; // Mark motor 3 as not moving reverse
            M_FWD[3] = 0; // Mark motor 4 as not moving forward
            M_REV[3] = 0; // Mark motor 4 as not moving reverse 
            break;
    }

    // Set duty cycle
    OCR1A = speed;
}
/*
void voltage_check(void){
    if (adc_read(0) < 360) { // If voltage is below 0.66V (assuming 3.3V reference)
                    motor_set(M_STOP, 0); // Stop the motor to prevent damage
                    return; // Ignore command if voltage is too low
            }
            else if (adc_read(0) > 650) { // If voltage is above 3.1V (assuming 3.3V reference, accounting for noise)
                    motor_set(M_STOP, 0); // Stop the motor to prevent damage
                    return; // Ignore command if voltage is too high
            }
}
            */

// --- Background Cooldown Math ---
ISR(TIMER2_OVF_vect) {

    static uint8_t ticks = 0;
    static uint8_t S_ticks[4] = {0, 0, 0, 0}; // Separate tick counters for each motor to track how long they've been active
    uint8_t S_active = 0; // Flag to track if any motor is active during this tick
    ticks++;

    // PWM is set to max if Motor is Active for more than 2s
    for (uint8_t i = 0; i < 4; i++) {
        if (M_Active[i] > 0) {
            S_active = 1; // At least one motor is active
            S_ticks[i]++;
            
            if (S_ticks[i] >= 61) { // 61 ticks ~ 2 seconds at ~61 Hz
                // Set PWM to max after 2s
               if (M_FWD[i] > 0) {
                    MAX_PWM = 1; // Max forward
                }
                else if (M_REV[i] > 0) {
                    MAX_PWM = 1; // Max reverse (same duty, just different direction)
                }
                S_ticks[i] = 61; // Cap switch tick counter so it doesn't overflow past 255
            }
        }
        else if (M_Active[i] == 0) {
            S_ticks[i] = 0; // Reset switch tick counter if motor is inactive
        }
    }

    if (S_active == 0) {
        MAX_PWM = 0; // Reset max PWM flag if no motors are active
    }
    
        if (ticks < 30) {
            return;
        }
    
        ticks = 0;
    
        // case: motor is active
    for (uint8_t i = 0; i < 4; i++){ 
        if (M_Active[i] > 0) {
            if (counters[i] < MAX_THRESHOLD){
                counters[i] += INC_VAL;
            }
            if (counters[i] >= MAX_THRESHOLD) {
            counters[i] = MAX_THRESHOLD; // Cap at max threshold
            STOP_M[i] = 1; // Set stop flag when threshold is reached
            }
        }
        else if (M_Active[i] == 0){ // case: motor is inactive
           if (counters[i] >= DEC_VAL) {
                counters[i] -= DEC_VAL;
                if (counters[i] < 0) {
                    counters[i] = 0; // Floor at zero
                }
            }
            if (counters[i] == 0) {
                STOP_M[i] = 0; // Clear stop flag when fully cooled down
            }
        }
    }
}


