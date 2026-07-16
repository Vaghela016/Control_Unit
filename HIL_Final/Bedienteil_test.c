#ifndef F_CPU
#define F_CPU 8000000UL 
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "i2cmaster.h"
#include "lcd_mccog42005a6w.h"
#include "M1_test.h"
#include "seat_pos.h"
// #include <avr/wdt.h>

#define BEDIENTEIL_ADDR 0x40
#define BEDIENTEIL_READ ((BEDIENTEIL_ADDR << 1) | I2C_READ)

void setup() {
    DDRD &= ~(1 << PD7); 
    DDRB |= (1 << PB0);
    PORTD &= ~(1 << PD7);
    PORTB |= (1 << PB0);
}

int main(void) {
   // MCUSR = 0; 
   // wdt_disable();

    // Initialize Hardware
    i2c_init();
    setup();
    timer_setup();
    PinChange_init();
    PinChange_setup();
    lcdBegin();
    motor_init();
    adc_init();
    SeatMemory_Init();

    // Application Variables
    volatile uint8_t current_state = 0; 
    volatile uint8_t last_state = 255; 
    volatile uint16_t speed = 0;
    
    // Sleep and Blanking Timers
    uint8_t keep_awake_timer = 0;
    uint8_t blanking_timer = 0; 
    
    // Background Event Tracking
    static uint8_t last_MAX_PWM = 0;
    static uint8_t last_stop_sum = 0;
    
    // End Position Memory Locks
    uint8_t END_FWD[4] = {0, 0, 0, 0};
    uint8_t END_REV[4] = {0, 0, 0, 0};

    // Open Motor connection Memory locks
    uint8_t OPEN[5] = {0, 0, 0, 0, 0};

    sei(); 
    _delay_ms(100); 

    lcdSetCursor(2, 3);
    lcdWriteText("POWER ON");
    _delay_ms(2000); 

    // Initial Voltage Check at Startup
        voltage_check(); // Check voltage before processing commands
        if (adc_read(0) < 360) { // If voltage is below ~9V
            lcdSetCursor(1, 1);
            lcdWriteText("E_07:");
            lcdSetCursor(2, 1);
            lcdWriteText("U_KL30 < 09");
            return 0; // Stop the program to prevent damage from low voltage
        }
        else if (adc_read(0) > 650) { // If voltage is above ~16V
            lcdSetCursor(1, 1);
            lcdWriteText("E_06:");
            lcdSetCursor(2, 1);
            lcdWriteText("U_KL30 > 16");
            return 0; // Stop the program to prevent damage from overvoltage
        }
    
    static uint8_t boss_mode_active = 0;

   // wdt_enable(WDTO_500MS);

    // --- MAIN EXECUTION LOOP ---
    while (1) {
        //wdt_reset();
         
        unsigned char ret = i2c_start(BEDIENTEIL_READ);

        // Initial Voltage Check at Startup
        voltage_check(); // Check voltage before processing commands
        if (adc_read(0) < 360) { // If voltage is below ~9V
            lcdSetCursor(1, 1);
            lcdWriteText("E_07:");
            lcdSetCursor(2, 1);
            lcdWriteText("U_KL30 < 09");
            return 0; // Stop the program to prevent damage from low voltage
        }
        else if (adc_read(0) > 650) { // If voltage is above ~16V
            lcdSetCursor(1, 1);
            lcdWriteText("E_06:");
            lcdSetCursor(2, 1);
            lcdWriteText("U_KL30 > 16");
            return 0; // Stop the program to prevent damage from overvoltage
        }
    
        // I2C Byte Decoding
        if (ret == 0) {
            // ACK RECEIVED
            uint8_t byte1 = i2c_readAck();
            uint8_t byte2 = i2c_readAck();
            uint8_t byte3 = i2c_readNak(); 
             
            i2c_stop();

            // --- Step 1: I2C Byte Decoding ---
            if      (byte1 & (1 << 1)) current_state = 1; 
            else if (byte1 & (1 << 0)) current_state = 2; 
            else if (byte1 & (1 << 3)) current_state = 3; 
            else if (byte1 & (1 << 2)) current_state = 4; 
            else if (byte1 & (1 << 5)) current_state = 5; 
            else if (byte1 & (1 << 4)) current_state = 6; 
            else if (byte1 & (1 << 7)) current_state = 7; 
            else if (byte1 & (1 << 6)) current_state = 8;
            else if (byte2 & (1 << 6)) current_state = 9; // Heating
            else if (byte2 & (1 << 5)) current_state = 10; // Boss Mode
            else                       current_state = 0; 

            // --- Step 1.5: End Position Directional Unlocking & Blanking ---
            // If the motor is driven in the opposite direction, clear the lock!
            if (current_state == 2) END_FWD[0] = 0;
            if (current_state == 1) END_REV[0] = 0;
            if (current_state == 4) END_FWD[1] = 0;
            if (current_state == 3) END_REV[1] = 0;
            if (current_state == 6) END_FWD[2] = 0;
            if (current_state == 5) END_REV[2] = 0;
            if (current_state == 8) END_FWD[3] = 0;
            if (current_state == 7) END_REV[3] = 0;

            // Start the 500ms inrush current blanking timer on any new switch press
            if ((current_state != 0 && current_state != last_state)  || (MAX_PWM != last_MAX_PWM)) {
                blanking_timer = 10; 
            }


            //  Seat Memory Kinematic Router
            uint8_t t3 = (byte2 & (1 << 7)) ? 1 : 0;
            uint8_t t4 = (byte3 & (1 << 0)) ? 1 : 0;
            uint8_t t5 = (byte3 & (1 << 1)) ? 1 : 0;
            
            // Pass LAST_STATE so the math tracker knows what the motor actually did
            uint8_t mem_override = SeatMemory_Run(current_state, last_state, t3, t4, t5, MAX_PWM, adc_avg(7));
            
            if (mem_override == 254) {
                last_state = 255; // 254 is a special code to force the LCD to redraw Idle safely
                current_state = 0;
            } else if (mem_override != 0) {
                current_state = mem_override; 
            }


            uint8_t current_stop_sum = STOP_M[0] + STOP_M[1] + STOP_M[2] + STOP_M[3];
            uint8_t open_count = OPEN[0] + OPEN[1] + OPEN[2] + OPEN[3] + OPEN[4];
            if (open_count == 5) {
                current_state = 0; // Force the state to IDLE to prevent further motor commands
                lcdSetCursor(1, 1); lcdWriteText(" E_00:");
                lcdSetCursor(2, 1); lcdWriteText("   ALL MOTORS OPEN   ");
            }
            else if (open_count > 0  && open_count < 5) {
                for (uint8_t i = 0; i < 4; i++) {
                    if (OPEN[i] == 1) {
                        current_state = 0; // Force the state to IDLE to prevent further motor commands
                        i = i + 1; // Increment to match the motor number (1-4)
                        lcdClear();
                        lcdSetCursor(1, 1); lcdWriteTextf(" E_0%u:", i);
                        lcdSetCursor(2, 1); lcdWriteTextf("MOT%u_OPEN", i);
                    }
                    else {
                        // Do nothing for motors that are not open
                    }
                }
                if (OPEN[4] == 1) {
                    current_state = 0; // Force the state to IDLE to prevent further motor commands 
                    lcdClear();
                    lcdSetCursor(1, 1); lcdWriteText(" E_05:");
                    lcdSetCursor(2, 1); lcdWriteText("HEAT_OPEN");
                }
            }

            // Boss Mode Non-Blocking Router

            // Safety Abort: If pressed any normal switch (1-9), cancel Boss Mode instantly!
            if (current_state >= 1 && current_state <= 9) {
                boss_mode_active = 0;
            }

            // Trigger: If pressed Taster 1, activate Boss Mode!
            if (current_state == 10) {
                boss_mode_active = 1;
            }

            // THE ROUTER: If Boss Mode is active, hijack the current_state!
            if (boss_mode_active == 1) {
                
                if (END_FWD[1] == 0) {
                    // Phase 1: Motor 2 is not at the end yet. Force state to 20.
                    current_state = 20; 
                } 
                else if (END_FWD[0] == 0) {
                    // Phase 2: Motor 2 is done, but Motor 1 is not. Force state to 21.
                    current_state = 21; 
                } 
                else {
                    // Phase 3: Both motors have hit their END_FWD flags!
                    boss_mode_active = 0; // Turn off the macro
                    current_state = 0;    // Drop back to IDLE
                }
            }

            // Change-Only State Machine Execution
            if ((current_state != last_state) || (MAX_PWM != last_MAX_PWM) || (current_stop_sum != last_stop_sum)) {
                
                lcdClear(); 
                speed = MAX_PWM ? PWM_MAX : (PWM_MAX / 2);
                
                if (current_state == 1) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 1 OBEN");
                    if (END_FWD[0] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M1_FWD, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 1 FWD     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[0] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 2) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 1 UNTEN");
                    
                    if (END_REV[0] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M1_REV, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 1 REV     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[0] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 3) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 2 OBEN");
                    
                    if (END_FWD[1] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M2_FWD, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 2 FWD     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[1] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 4) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 2 UNTEN");
                    
                    if (END_REV[1] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M2_REV, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 2 REV     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[1] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 5) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 3 OBEN");
                    
                    if (END_FWD[2] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M3_FWD, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 3 FWD     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[2] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 6) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 3 UNTEN");
                    
                    if (END_REV[2] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M3_REV, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 3 REV     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[2] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 7) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 4 OBEN");
                    
                    if (END_FWD[3] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M4_FWD, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 4 FWD     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[3] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 8) {
                    lcdSetCursor(0, 1); lcdWriteText("Schalter 4 UNTEN");
                    
                    if (END_REV[3] == 1) { 
                        motor_set(M_STOP, 0); 
                        lcdSetCursor(1, 1); lcdWriteText("E_08:");
                        lcdSetCursor(2, 1); lcdWriteText("   t_safety   ");
                    } else { 
                        motor_set(M4_REV, speed);
                        lcdSetCursor(1, 1); lcdWriteText("Motor 4 REV     ");
                        if (MAX_PWM == 1) { lcdSetCursor(3, 1); lcdWriteText("SPEED: 100%     "); }
                        else { lcdSetCursor(3, 1); lcdWriteText("SPEED: 50%      "); }
                    }
                    if (STOP_M[3] == 1) { lcdSetCursor(2, 1); lcdWriteText("MOTOR COOLING DOWN"); }
                }
                else if (current_state == 9) {
                    lcdSetCursor(0, 1); lcdWriteText("Taster 2");
                    lcdSetCursor(1, 1); lcdWriteText("HEATING..");
                    motor_set(H_ON, speed);
                }
                else if (current_state == 20) {
                    lcdSetCursor(0, 1); lcdWriteText("Taster 1");
                    lcdSetCursor(1, 1); lcdWriteText("    BOSS MODE    ");
                    motor_set(M2_FWD, speed);
                }
                else if (current_state == 21) {
                    lcdSetCursor(0, 1); lcdWriteText("Taster 1");
                    lcdSetCursor(1, 1); lcdWriteText("    BOSS MODE    ");
                    motor_set(M1_FWD, speed);
                }
                else if (current_state == 22) {
                    lcdSetCursor(0, 1); lcdWriteText("= MEMORY TARGET =");
                    motor_set(M1_FWD, speed);
                    lcdSetCursor(1, 1); lcdWriteTextf("POS: %ld", SeatMemory_GetPosition());
                }
                else if (current_state == 23) {
                    lcdSetCursor(0, 1); lcdWriteText("= MEMORY TARGET =");
                    motor_set(M1_REV, speed);
                    lcdSetCursor(1, 1); lcdWriteTextf("POS: %ld", SeatMemory_GetPosition());
                }
                else if (current_state == 24 || current_state == 25) {
                    lcdSetCursor(1, 1); lcdWriteText("   WELCOME...   ");
                    if (current_state == 24) motor_set(M1_REV, speed);
                    else motor_set(M1_FWD, speed);
                }
                
                // FEEDBACK
                else if (current_state >= 33 && current_state <= 35) {
                    motor_set(M_STOP, 0);
                    lcdSetCursor(0, 1); lcdWriteText("= MEMORY SETUP =");
                    lcdSetCursor(1, 1); lcdWriteTextf("POS %u SAVED    ", current_state - 30);
                }
                else if (current_state >= 43 && current_state <= 45) {
                    motor_set(M_STOP, 0);
                    lcdSetCursor(0, 1); lcdWriteText("= MEMORY SETUP =");
                    lcdSetCursor(1, 1); lcdWriteTextf("POS %u DELETED  ", current_state - 40);
                }
                else if (current_state == 50) { // The 10s Fallback Output
                    motor_set(M_STOP, 0);
                    lcdSetCursor(0, 1); lcdWriteText("                ");
                    lcdSetCursor(1, 1); lcdWriteText("E11: Seat Memory");
                }
                
                else {
                    motor_set(M_STOP, 0);
                    // Print relative seat position when idle
                    lcdSetCursor(1, 1); lcdWriteTextf("POS: %ld        ", SeatMemory_GetPosition());
                }

                // Update histories
                last_state = current_state;
                last_MAX_PWM = MAX_PWM;
                last_stop_sum = current_stop_sum;
            }
        } 
        else if (ret != 0) {
            lcdClear();
            lcdSetCursor(1, 7);
            lcdWriteText("NACK");
            i2c_stop();
        }

        // Active Polling & Blanking Delay
        _delay_ms(50);
        
        if (keep_awake_timer > 0) keep_awake_timer--;
        if (blanking_timer > 0) blanking_timer--; 

        // Step 3: Continuous End Position Monitoring
        if (current_state != 0 && blanking_timer == 0) {

            uint8_t fault_active = 0;

            if (SeatMemory_IsCalibrating() == 1) {
                 // Do absolutely nothing. Let seat_pos.c handle the wall impact natively.
            }

            // CRITICAL SAFETY SHIELD
            else {
                if      (current_state == 1 && (END_FWD[0] == 1 || OPEN[0] == 1)) fault_active = 1;
                else if (current_state == 2 && (END_REV[0] == 1 || OPEN[0] == 1)) fault_active = 1;
                else if (current_state == 3 && (END_FWD[1] == 1 || OPEN[1] == 1)) fault_active = 1;
                else if (current_state == 4 && (END_REV[1] == 1 || OPEN[1] == 1)) fault_active = 1;
                else if (current_state == 5 && (END_FWD[2] == 1 || OPEN[2] == 1)) fault_active = 1;
                else if (current_state == 6 && (END_REV[2] == 1 || OPEN[2] == 1)) fault_active = 1;
                else if (current_state == 7 && (END_FWD[3] == 1 || OPEN[3] == 1)) fault_active = 1;
                else if (current_state == 8 && (END_REV[3] == 1 || OPEN[3] == 1)) fault_active = 1;
                else if (current_state == 9 && (OPEN[4] == 1)) fault_active = 1;
                else if (current_state == 20 && (END_FWD[1] == 1 || OPEN[1] == 1)) fault_active = 1;
                else if (current_state == 21 && (END_FWD[0] == 1 || OPEN[0] == 1)) fault_active = 1;
            }


            // ONLY evaluate the hardware if the motor is actually running!
            if (fault_active == 0) {
                
                // Read the ADC exactly ONCE per loop
                uint16_t current_sense = adc_avg(7);

                // STALL DETECT: End Position Reached
                if (current_sense >= 303) {
                    
                    if (current_state == 1 || current_state == 21) END_FWD[0] = 1; 
                    else if (current_state == 2) END_REV[0] = 1;
                    else if (current_state == 3 || current_state == 20) END_FWD[1] = 1;
                    else if (current_state == 4) END_REV[1] = 1;
                    else if (current_state == 5) END_FWD[2] = 1;
                    else if (current_state == 6) END_REV[2] = 1;
                    else if (current_state == 7) END_FWD[3] = 1;
                    else if (current_state == 8) END_REV[3] = 1;
                    
                    motor_set(M_STOP, 0); 
                    last_state = 0; // Force LCD redraw
                }

                // OPEN LOAD DETECT: Wire broken or motor disconnected 
                else if (current_sense < 25) { 
                    
                    if (current_state == 1 || current_state == 2 || current_state == 21) OPEN[0] = 1;
                    else if (current_state == 3 || current_state == 4 || current_state == 20) OPEN[1] = 1;
                    else if (current_state == 5 || current_state == 6) OPEN[2] = 1;
                    else if (current_state == 7 || current_state == 8) OPEN[3] = 1;
                    else if (current_state == 9) OPEN[4] = 1; 
                    
                    motor_set(M_STOP, 0); 
                    last_state = 0;     
                }
            }
        }

        // Sleep Mode 
        if (idle() == 1 && keep_awake_timer == 0) {
            
            keep_awake_timer = 10;
            
            lcdClear();
            lcdSetCursor(1, 1);
            lcdWriteText("NAP TIME");
            
            // Allow physical LCD pixels to draw
            _delay_ms(1000); 
            
            MCU_sleep();
            
            last_state = 255; 
        }
    }
    
    return 0;
}