#ifndef SEAT_POS_H
#define SEAT_POS_H

#include <stdint.h>

void SeatMemory_Init(void); // Initialize the seat memory system, load last position from EEPROM

// Check if the seat memory system is currently in calibration mode (welcome feature)
uint8_t SeatMemory_Run(uint8_t manual_state, uint8_t physical_last_state, uint8_t taster3, uint8_t taster4, uint8_t taster5, uint8_t max_pwm, uint16_t adc_val);

// Check if the seat memory system is currently in calibration mode (welcome feature)
uint8_t SeatMemory_IsCalibrating(void);

// Get the current relative position of the seat
int32_t SeatMemory_GetPosition(void);

#endif