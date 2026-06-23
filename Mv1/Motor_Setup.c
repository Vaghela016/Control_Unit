#ifndef F_CPU
#define F_CPU 8000000UL // Define clock speed as 8 MHz
#endif

#include <avr/io.h>
#include <util/delay.h>

// Define hardware pins
#define PWM1 PB1 // OC1A (Arduino Pin 9)
#define PWM2 PB2 // OC1B (Arduino Pin 10)
#define M1_F PD0 // Arduino Pin 2 (M1+)
#define M2_F PD1 // Arduino Pin 3 (M2+)
#define M3_F PD2 // Arduino Pin 4  (M3+)
#define M4_F PD3 // Arduino Pin 5  (M4+)
#define M12_R PD1 // Arduino Pin 3 - (M1-)(M2+)
#define M23_R PD2 // Arduino Pin 4 - (M2-)(M3+)
#define M34_R PD3 // Arduino Pin 5 - (M3-)(M4+)
#define M4_R PD4 // Arduino Pin 6 - (M4-)

// Motor states
#define M_STOP 0
#define M1_FWD 1
#define M2_FWD 2
#define M3_FWD 3
#define M4_FWD 4
#define M1_REV 5
#define M2_REV 6
#define M3_REV 7
#define M4_REV 8

void motor_init(void) {
    // 1. Set pins as outputs
    DDRB |= (1 << PWM);
    DDRD |= ((1 << M1_F) | (1 << M2_F) | (1 << M3_F) \
            | (1 << M4_F) | (1 << M12_R) | (1 << M23_R) \
            | (1 << M34_R) | (1 << M4_R));
            

    // 2. Configure Timer1 for Fast PWM, Mode 14 (TOP = ICR1)
    // Clear OC1A on Compare Match, set OC1A at BOTTOM (non-inverting mode)
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    
    // Prescaler = 1 (CS10 = 1), WGM12 = 1, WGM13 = 1
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS10);

    // 3. Set the TOP value for 20 kHz
    ICR1 = 399; 

    // 4. Initialize with 0 speed
    OCR1A = 0; 
}

// Speed should be between 0 and 399
void motor_set(uint8_t state, uint16_t speed) {
    // Cap the speed to our TOP value to prevent overflow
    if (speed > 399) {
        speed = 399;
    }

    // Set motor direction
    switch (state) {
        
        case M1_FWD:
            PORTD |= (1 << M1_F);   //  HIGH
            //PORTD &= ~((1 << M31_R) | (1 << M3_F));  // LOW
            break;
          
        case M2_FWD:
            PORTD |= ((1 << M2_F) | (1 << M1_F));   //  HIGH
            //PORTD &= ~((1 << M24_R) | (1 << M4_F));  // LOW
            break;

        case M3_FWD:
            PORTD |= ((1 << M3_F) | (1 << M2_F));   //  HIGH
            //PORTD &= ~((1 << M31_R) | (1 << M1_F));  // LOW
            break;

        case M4_FWD:
            PORTD |= (1 << M4_F) | (1 << M3_F);   //  HIGH
            //PORTD &= ~((1 << M24_R) | (1 << M2_F));  // LOW
            break;

        case M1_REV:
            //PORTD &= ~(1 << M1_F);  // LOW
            PORTD |= ((1 << M12_R) | (1 << M3_F)); // HIGH
            break;
            
        case M2_REV:
            //PORTD &= ~(1 << M2_F);  // LOW
            PORTD |= ((1 << M23_R) | (1 << M4_F)); // HIGH
            break;

        case M3_REV:
            //PORTD &= ~(1 << M3_F);  // LOW
            PORTD |= ((1 << M34_R) | (1 << M4_R)); // HIGH
            break;

        case M4_REV:
            //PORTD &= ~(1 << M4_F);  // LOW
            PORTD |= (1 << M4_R); // HIGH
            break;

        case M_STOP:
        default:
            PORTD &= ~((1 << M1_F) | (1 << M2_F) | (1 << M3_F) | \
                      (1 << M4_F) | (1 << M31_R) | (1 << M24_R));  //LOW
            break;
    }

    // Set duty cycle
    OCR1A = speed;
}
