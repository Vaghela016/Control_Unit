#ifndef LCD_MCCOG42005A6W_H
#define LCD_MCCOG42005A6W_H

/*
  MCCOG42005A6W / similar I2C character LCD driver for AVR (Atmel Studio).

  - Uses AVR TWI hardware (I2C master).
  - Sends every data character in its own I2C transaction for compatibility
    with controllers that only latch the first data byte in a streamed write.

  Configuration (override in your project or before including this header):
    - LCD_I2C_ADDR: 7-bit I2C address (default 0x3C)
    - LCD_TWI_HZ:   I2C clock (default 100000)
    - LCD_PRINTF_BUF: size for formatted printing buffer (default 64)
    - LCD_ROWx_ADDR: DDRAM start address per row (defaults match your current usage)
*/

#include <stdint.h>
#include <stdbool.h>

#ifndef LCD_I2C_ADDR
#define LCD_I2C_ADDR 0x3C
#endif

#ifndef LCD_TWI_HZ
#define LCD_TWI_HZ 25000UL
#endif

#ifndef LCD_PRINTF_BUF
#define LCD_PRINTF_BUF 64
#endif

/* Default row start addresses. Row index is 0-based. */
#ifndef LCD_ROW0_ADDR
#define LCD_ROW0_ADDR 0x00
#endif

#ifndef LCD_ROW1_ADDR
#define LCD_ROW1_ADDR 0x18
#endif

#ifndef LCD_ROW2_ADDR
#define LCD_ROW2_ADDR 0x30
#endif

#ifndef LCD_ROW3_ADDR
#define LCD_ROW3_ADDR 0x48
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --- TWI (I2C) --- */
void lcdI2CInit(uint32_t scl_hz);
bool lcdI2CIsInitialized(void);

/* --- Low-level LCD writes --- */
void lcdWriteIns(uint8_t cmd);
void lcdWriteData(uint8_t data);

/* --- High-level helpers --- */
void lcdClear(void);               /* Clear display (DDRAM), no hardware reset */
void lcdSetDDRAM(uint8_t addr);    /* Set DDRAM address (uses 0x90 + addr as in your snippet) */
void lcdSetLine(uint8_t row);      /* Set cursor to start of row (row is 0-based) */
void lcdSetCursor(uint8_t row, uint8_t col); /* Set cursor (row/col are 0-based) */
/* Load a custom 5x8 glyph into CGRAM slot 0..7 (call lcdSetCursor after this) */
void lcdCreateCustomChar(uint8_t slot, const uint8_t pattern[8]);
/* Write custom char from CGRAM slot 0..7 at current cursor position */
void lcdWriteCustomChar(uint8_t slot);
void lcdWriteText(const char *s);  /* Write RAM string */

/* printf-style: lcdWriteTextf("Counter: %d", counter); */
void lcdWriteTextf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Convenience for numbers (no overloading in C) */
void lcdWriteU32(uint32_t value);
void lcdWriteU32Padded(uint32_t value, uint8_t pad_to_width);

/* Init sequence copied from your working Arduino/ESP32 sketch */
void lcdInitFromDatasheet(void);

/* One-call convenience: init TWI at LCD_TWI_HZ and run init sequence */
static inline void lcdBegin(void) {
  if (!lcdI2CIsInitialized()) {
    lcdI2CInit(LCD_TWI_HZ);
  }
  lcdInitFromDatasheet();
}

#ifdef __cplusplus
}
#endif

#endif /* LCD_MCCOG42005A6W_H */
