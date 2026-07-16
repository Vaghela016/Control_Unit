#include "M1_test.h"

#ifndef F_CPU
    #define F_CPU 8000000UL 
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
// #include <avr/wdt.h>
#include "i2cmaster.h"
#include "lcd_mccog42005a6w.h"

// Global Tracking Arrays
volatile int16_t counters[4] = {0, 0, 0, 0}; 
volatile uint8_t STOP_M[4]   = {0, 0, 0, 0}; 
volatile uint8_t M_Active[4] = {0, 0, 0, 0}; 
volatile uint8_t M_FWD[4]    = {0, 0, 0, 0};
volatile uint8_t M_REV[4]    = {0, 0, 0, 0};
volatile uint8_t Wait[4]     = {0, 0, 0, 0}; 
volatile uint8_t MAX_PWM     = 0;

// Hardware Initialization

void adc_init(void) {
    ADMUX = (1 << REFS0); // AVCC as reference
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // 128 Prescaler
    DIDR0 |= (1 << ADC2D) | (1 << ADC3D); // Power saving on unused analog pins
}

uint16_t adc_read(uint8_t channel) {
    channel &= 0x07; 
    ADMUX = (ADMUX & 0xF8) | channel;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

uint16_t adc_avg(uint8_t channel) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
        sum += adc_read(channel);
    }
    return sum / 4; 
}

void timer_setup() {
    // TIMER 2: Cooldown tick (~61 Hz)
    TCCR2A = 0;  
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
    TIMSK2 = (1 << TOIE2); 
}

void motor_init(void) {
    // Configure Motor Control Pins as Outputs
    DDRD |= (1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);
    
    // Configure PWM Pins (OC1A/PB1 and OC1B/PB2) as Outputs
    DDRB |= (1 << PB1) | (1 << PB2);
    
    // Configure the Current Sense / SEL0 Pin as Output
    DDRE |= (1 << PE0);
    
    // Initialize all outputs to LOW
    PORTD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
    PORTE &= ~(1 << PE0);
    
    // Hardware Timer1 Setup for Fast PWM (Mode 14)
    // Non-inverting mode on Channel A and B
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
    // Fast PWM with ICR1 as TOP, No Prescaler (8MHz / 1 = 8MHz timer clock)
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS10);
    
    ICR1 = PWM_MAX; // Sets the default frequency to ~20 kHz
    OCR1A = 0;      // 0% Duty Cycle
    OCR1B = 0;      // 0% Duty Cycle
}

void PinChange_init(void){
    DDRD &= ~(1 << PD6); 
    PORTD |= (1 << PD6); // Enable pull-up resistor on PD6 for I2C wake-up
}

void PinChange_setup(void){
    PCICR |= (1 << PCIE2);    
    PCMSK2 |= (1 << PCINT22); 
}

void MCU_sleep(void) {
    // wdt_disable(); // Disable watchdog to prevent reboot while asleep
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
    sleep_enable(); 
    sleep_cpu();   // System HALTS here until Pin Change triggers
    sleep_disable(); 
    //wdt_enable(WDTO_250MS); // Re-arm watchdog instantly upon waking up
    _delay_ms(10); // Sync delay for Bedienteil processing
}

ISR(PCINT2_vect) {
    // Empty ISR mandatory for routing the wake-up interrupt
}

// Motor Control Logic

void motor_set(uint8_t state, uint16_t speed) {
    
    // Scale Timer1 TOP value to switch between 10kHz (Heat) and 20kHz (Motors)
    if (state == H_ON) {
        ICR1 = hot; // 799 (~10 kHz)
        speed = speed * 2; // Scale the duty cycle to match the new TOP
    } else {
        ICR1 = PWM_MAX; // 399 (~20 kHz)
    }   

    switch (state) {
        
        case M1_FWD:
            if (STOP_M[0] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= (1 << PD0);
            PORTD &= ~((1 << PD1) | (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
            PORTE |= (1 << PE0); 
            OCR1A = speed; OCR1B = 0;
            M_Active[0] = 1; M_FWD[0] = 1; M_REV[0] = 0;
            break;
          
        case M1_REV:
            if (STOP_M[0] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= ((1 << PD1) | (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
            PORTD &= ~(1 << PD0);
            PORTE &= ~(1 << PE0);
            OCR1A = speed; OCR1B = 0;
            M_Active[0] = 1; M_FWD[0] = 0; M_REV[0] = 1;
            break;

        case M2_FWD:
            if (STOP_M[1] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= ((1 << PD1) | (1 << PD0));
            PORTD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
            PORTE &= ~(1 << PE0);
            OCR1A = speed; OCR1B = 0;
            M_Active[1] = 1; M_FWD[1] = 1; M_REV[1] = 0;
            break;

        case M2_REV:
            if (STOP_M[1] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= ((1 << PD2) | (1 << PD3) | (1 << PD4));
            PORTD &= ~((1 << PD0) | (1 << PD1) | (1 << PD5));
            PORTE |= (1 << PE0);
            OCR1A = speed; OCR1B = 0;
            M_Active[1] = 1; M_FWD[1] = 0; M_REV[1] = 1;
            break;

        case M3_FWD:
            if (STOP_M[2] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= ((1 << PD0) | (1 << PD1) | (1 << PD2));
            PORTD &= ~((1 << PD3) | (1 << PD4) | (1 << PD5));
            PORTE |= (1 << PE0);
            OCR1A = speed; OCR1B = 0;
            M_Active[2] = 1; M_FWD[2] = 1; M_REV[2] = 0;
            break;

        case M3_REV:
            if (STOP_M[2] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= ((1 << PD3) | (1 << PD4));
            PORTD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD5));
            PORTE &= ~(1 << PE0);
            OCR1A = speed; OCR1B = 0;
            M_Active[2] = 1; M_FWD[2] = 0; M_REV[2] = 1;
            break;

        case M4_FWD:
            if (STOP_M[3] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= ((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3));
            PORTD &= ~((1 << PD4) | (1 << PD5));
            PORTE &= ~(1 << PE0);
            OCR1A = speed; OCR1B = speed; // M4 requires Dual PWM
            M_Active[3] = 1; M_FWD[3] = 1; M_REV[3] = 0;
            break;

        case M4_REV:
            if (STOP_M[3] == 1) { motor_set(M_STOP, 0); return; }
            PORTD |= ((1 << PD4) | (1 << PD5));
            PORTD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3));
            PORTE |= (1 << PE0);
            OCR1A = speed; OCR1B = speed; // M4 requires Dual PWM
            M_Active[3] = 1; M_FWD[3] = 0; M_REV[3] = 1;
            break;
        
        case H_ON:
            PORTD |= (1 << PD5);
            PORTD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3) | (1 << PD4));  
            PORTE &= ~(1 << PE0); 
            OCR1A = 0; OCR1B = speed;
            break;

        case M_STOP:
        default:
            OCR1A = 0; OCR1B = 0;
            PORTD &= ~((1 << PD5) | (1 << PD4) | (1 << PD3) | (1 << PD2) | (1 << PD1) | (1 << PD0));  
            PORTE &= ~(1 << PE0); 
            
            for(uint8_t i = 0; i < 4; i++){
                M_Active[i] = 0; M_FWD[i] = 0; M_REV[i] = 0;
            }
            break;
    }
}

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

// Background Thermal/Acceleration Management

ISR(TIMER2_OVF_vect) {
    static uint8_t ticks = 0;
    static uint8_t S_ticks[4] = {0, 0, 0, 0}; 
    uint8_t S_active = 0; 
    ticks++;

    // Track active time for the 2-second PWM acceleration
    for (uint8_t i = 0; i < 4; i++) {
        if (M_Active[i] > 0) {
            S_active = 1; 
            S_ticks[i]++;
            
            if (S_ticks[i] >= 61) { // 61 ticks = ~2 seconds at ~61 Hz
                if (M_FWD[i] > 0 || M_REV[i] > 0) MAX_PWM = 1; 
                S_ticks[i] = 61; 
            }
        }
        else if (M_Active[i] == 0) {
            S_ticks[i] = 0; 
        }
    }

    if (S_active == 0) MAX_PWM = 0; 
    if (ticks < 30) return;
    ticks = 0;
    
    // Process 1-second Cooldown ticks
    for (uint8_t i = 0; i < 4; i++){ 
        if (M_Active[i] > 0) {
            if (counters[i] < MAX_THRESHOLD) counters[i] += INC_VAL;
            if (counters[i] >= MAX_THRESHOLD) {
                counters[i] = MAX_THRESHOLD; 
                STOP_M[i] = 1; 
            }
        }
        else if (M_Active[i] == 0){ 
           if (counters[i] >= DEC_VAL) counters[i] -= DEC_VAL;
           if (counters[i] < 0) counters[i] = 0; 
           if (counters[i] == 0) STOP_M[i] = 0; 
        }
    }
}

uint8_t idle(void) {
    for (uint8_t i = 0; i < 4; i++) {
        if (M_Active[i] > 0 || counters[i] > 0) return 0; // Busy processing cooldown
    }
    if (!(PIND & (1 << PD6))) return 0; // Busy receiving I2C
    return 1; 
}