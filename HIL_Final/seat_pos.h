#ifndef SEAT_POS_H
#define SEAT_POS_H

#include <stdint.h>

void SeatMemory_Init(void);

// Added 'physical_last_state' so the tracker knows exactly what the motor did
uint8_t SeatMemory_Run(uint8_t manual_state, uint8_t physical_last_state, uint8_t taster3, uint8_t taster4, uint8_t taster5, uint8_t max_pwm, uint16_t adc_val);

uint8_t SeatMemory_IsCalibrating(void);
int32_t SeatMemory_GetPosition(void);

#endif