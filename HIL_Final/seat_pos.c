#include "seat_pos.h"
#include <avr/eeprom.h>
#include <util/delay.h>
#include "lcd_mccog42005a6w.h"
#include "M1_test.h" 

// EEPROM
int32_t EEMEM ee_last_pos = 0;
int32_t EEMEM ee_pos_T3 = -1;
int32_t EEMEM ee_pos_T4 = -1;
int32_t EEMEM ee_pos_T5 = -1;

// Virtual Position Tracking
static int32_t virtual_position = 0;
static int32_t target_position = 0;
static int32_t startup_target = 0;

// 0=Boot, 1=Welcome_REV, 2=Welcome_FWD, 3=Normal, 4=Recall, 5=Error
static uint8_t memory_status = 0; 
static uint16_t welcome_timeout = 0;
static uint8_t btn_timers[3] = {0, 0, 0};

void SeatMemory_Init(void) {
    startup_target = eeprom_read_dword((uint32_t*)&ee_last_pos);
    if (startup_target == -1 || startup_target < 0) {
        startup_target = 0; 
    }
    memory_status = 1; 
    welcome_timeout = 0;
}

uint8_t SeatMemory_IsCalibrating(void) {
    if (memory_status == 1 || memory_status == 2) return 1;
    return 0;
}

int32_t SeatMemory_GetPosition(void) {
    return virtual_position;
}

uint8_t SeatMemory_Run(uint8_t manual_state, uint8_t physical_last_state, uint8_t t3, uint8_t t4, uint8_t t5, uint8_t max_pwm, uint16_t adc_val) {
    static uint8_t last_active_motor = 0;

    // KINEMATIC POSITION TRACKING
    uint8_t active_fwd = (physical_last_state == 1 || physical_last_state == 21 || physical_last_state == 22 || physical_last_state == 25);
    uint8_t active_rev = (physical_last_state == 2 || physical_last_state == 23 || physical_last_state == 24);

    if (active_fwd) virtual_position += (max_pwm ? 2 : 1);
    else if (active_rev) virtual_position -= (max_pwm ? 2 : 1);

    if (active_fwd || active_rev) {
        last_active_motor = 1;
    } else if (last_active_motor == 1) {
        eeprom_write_dword((uint32_t*)&ee_last_pos, virtual_position);
        last_active_motor = 0;
    }

    // STATE MACHINE ROUTING

    // PHASE 1: Welcome Feature (Drive to physical end-stop)
    if (memory_status == 1) {
        welcome_timeout++;
        if (welcome_timeout > 10) { 
            if (adc_val >= 303) {
                virtual_position = 0; 
                memory_status = 2;    
                welcome_timeout = 0;  
                return 0; 
            }
            if (welcome_timeout > 200) { 
                memory_status = 5; 
                welcome_timeout = 0; // Reset timer for the E11 display
                return 0;
            }
        }
        return 24; // Force Welcome REV
    }
    
    // PHASE 2: Welcome Feature (Return to pre-power-off position)
    else if (memory_status == 2) {
        welcome_timeout++;
        
        if (virtual_position >= startup_target) {
            memory_status = 3; 
            return 0; 
        }
        if (welcome_timeout > 200) { 
            memory_status = 5; 
            welcome_timeout = 0; // Reset timer for the E11 display
            return 0;
        }
        if (welcome_timeout > 10 && adc_val >= 303) {
            memory_status = 3; 
            return 0;
        }
        
        return 25; // Force Welcome FWD
    }
    
    // PHASE 3: Error Handling (Non-Blocking Auto-Recovery)
    else if (memory_status == 5) {
        welcome_timeout++;
        
        // Hold the E11 state for exactly 40 loops (2.0 seconds)
        if (welcome_timeout <= 40) {
            return 50; 
        } else {
            // Timer finished! Release the lock and return to normal operation.
            memory_status = 3; 
            welcome_timeout = 0; 
            return 254; // Tell the main loop to wipe the LCD and return to IDLE
        }
    }
    
    // PHASE 4: Normal Operation & Memory Buttons
    else if (memory_status == 3 || memory_status == 4) {
        
        if (manual_state >= 1 && manual_state <= 9) memory_status = 3;

        uint8_t btns[3] = {t3, t4, t5};
        int32_t* addrs[3] = {(int32_t*)&ee_pos_T3, (int32_t*)&ee_pos_T4, (int32_t*)&ee_pos_T5};

        for (uint8_t i = 0; i < 3; i++) {
            if (btns[i]) {
                btn_timers[i]++;
                
                // Write to EEPROM exactly at the boundaries
                if (btn_timers[i] == 40) eeprom_write_dword((uint32_t*)addrs[i], virtual_position);
                else if (btn_timers[i] == 100) eeprom_write_dword((uint32_t*)addrs[i], -1);
                
                // Hijack the main loop to display the feedback instantly while holding!
                if (btn_timers[i] >= 100) return 43 + i;      // Force State 43, 44, or 45 (DELETED)
                else if (btn_timers[i] >= 40) return 33 + i;  // Force State 33, 34, or 35 (SAVED)
                
            } else if (btn_timers[i] > 0) {
                if (btn_timers[i] < 40) { // Quick Tap (< 2s)
                    target_position = eeprom_read_dword((uint32_t*)addrs[i]);
                    if (target_position != -1) memory_status = 4;
                } 
                btn_timers[i] = 0;
            }
        }

        if (memory_status == 4) {
            int32_t error = target_position - virtual_position;
            if (error > 2) return 22;      
            else if (error < -2) return 23;
            else memory_status = 3;        
        }
    }
    return 0; 
}