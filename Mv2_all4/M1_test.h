#ifndef M1_TEST
	#define M1_TEST

#ifndef F_CPU
	#define F_CPU 8000000UL // Define clock speed as 8 MHz
#endif

#include <avr/io.h>
#include <util/delay.h>

// Define hardware pins

#ifndef PWM1
	#define PWM1 PB1 // OC1A (Arduino Pin 9)
#endif

#ifndef PWM2
	#define PWM2 PB2 // OC1B (Arduino Pin 10)
#endif

#ifndef M1_F
	#define M1_F PD0 // Arduino Pin 2 (M1+)
#endif


#ifndef M2_F
	#define M2_F PD1 // Arduino Pin 3 (M2+)
#endif


#ifndef M3_F
	#define M3_F PD2 // Arduino Pin 4  (M3+)
#endif


#ifndef M4_F
	#define M4_F PD3 // Arduino Pin 5  (M4+)
#endif


#ifndef M4_R
	#define M4_R PD4 // Arduino Pin 6 - (M4-)
#endif



// Motor states

#ifndef M_STOP
	#define M_STOP 0
#endif


#ifndef M1_FWD
	#define M1_FWD 1
#endif


#ifndef M2_FWD
	#define M2_FWD 2
#endif


#ifndef M3_FWD
	#define M3_FWD 3
#endif


#ifndef M4_FWD
	#define M4_FWD 4
#endif


#ifndef M1_REV
	#define M1_REV 5
#endif


#ifndef M2_REV
	#define M2_REV 6
#endif


#ifndef M3_REV
	#define M3_REV 7
#endif


#ifndef M4_REV
	#define M4_REV 8
#endif


void motor_init(void); // Motor PINs Setup

void motor_set(uint8_t state, uint16_t speed); // Motor Drive


#endif