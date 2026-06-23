#include "lcd_mccog42005a6w.h"

#include <avr/io.h>

#ifndef F_CPU
/* Define F_CPU in your project settings for correct delays and TWI speed. */
#define F_CPU 8000000UL
#endif

#include <util/delay.h>
#include <util/twi.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- Minimal TWI master (write-only) ---- */

static bool twi_start(uint8_t sla_w) {
  TWCR = (uint8_t)((1U << TWINT) | (1U << TWSTA) | (1U << TWEN));
  while (!(TWCR & (1U << TWINT))) {}

  uint8_t status = (uint8_t)(TWSR & 0xF8);
  if ((status != TW_START) && (status != TW_REP_START)) return false;

  TWDR = sla_w;
  TWCR = (uint8_t)((1U << TWINT) | (1U << TWEN));
  while (!(TWCR & (1U << TWINT))) {}

  status = (uint8_t)(TWSR & 0xF8);
  return (status == TW_MT_SLA_ACK);
}

static bool twi_write(uint8_t data) {
  TWDR = data;
  TWCR = (uint8_t)((1U << TWINT) | (1U << TWEN));
  while (!(TWCR & (1U << TWINT))) {}

  uint8_t status = (uint8_t)(TWSR & 0xF8);
  return (status == TW_MT_DATA_ACK);
}

static void twi_stop(void) {
  TWCR = (uint8_t)((1U << TWINT) | (1U << TWEN) | (1U << TWSTO));
  /* TWSTO is cleared by hardware. No need to wait. */
}

static bool twi_tx(uint8_t addr7, const uint8_t *bytes, uint8_t len) {
  if (len == 0) return true;

  if (!twi_start((uint8_t)((addr7 << 1) | 0U))) {
    twi_stop();
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    if (!twi_write(bytes[i])) {
      twi_stop();
      return false;
    }
  }

  twi_stop();
  return true;
}

void lcdI2CInit(uint32_t scl_hz) {
  /* prescaler = 1 */
  TWSR = 0;

  if (scl_hz == 0) scl_hz = LCD_TWI_HZ;

  /* SCL = F_CPU / (16 + 2*TWBR*prescaler) */
  uint32_t twbr = ((uint32_t)F_CPU / scl_hz);
  if (twbr >= 16) twbr = (twbr - 16) / 2;
  else twbr = 0;
  if (twbr > 255) twbr = 255;

  TWBR = (uint8_t)twbr;
  TWCR = (uint8_t)(1U << TWEN);
}

bool lcdI2CIsInitialized(void) {
  return (TWCR & (1U << TWEN)) != 0;
}

/* ---- LCD ---- */

void lcdWriteIns(uint8_t cmd) {
  uint8_t buf[2] = { 0x00, cmd };
  (void)twi_tx(LCD_I2C_ADDR, buf, (uint8_t)sizeof(buf));
}

void lcdWriteData(uint8_t data) {
  uint8_t buf[2] = { 0x40, data };
  (void)twi_tx(LCD_I2C_ADDR, buf, (uint8_t)sizeof(buf));
}

void lcdWriteText(const char *s) {
  /* Most compatible: 1 character per I2C transaction. */
  while (s && *s) {
    lcdWriteData((uint8_t)*s++);
  }
}

void lcdWriteTextf(const char *fmt, ...) {
  char buf[LCD_PRINTF_BUF];
  va_list ap;
  va_start(ap, fmt);
  (void)vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  lcdWriteText(buf);
}

void lcdWriteU32(uint32_t value) {
  char buf[12];
  ultoa(value, buf, 10);
  lcdWriteText(buf);
}

void lcdWriteU32Padded(uint32_t value, uint8_t pad_to_width) {
  char buf[32];
  ultoa(value, buf, 10);

  if (pad_to_width > 0) {
    if (pad_to_width >= sizeof(buf)) pad_to_width = (uint8_t)(sizeof(buf) - 1);
    uint8_t n = (uint8_t)strlen(buf);
    if (n < pad_to_width) {
      memset(buf + n, ' ', (size_t)(pad_to_width - n));
      buf[pad_to_width] = '\0';
    }
  }

  lcdWriteText(buf);
}

void lcdClear(void) {
  lcdWriteIns(0x01);
  _delay_ms(20);
}

void lcdSetDDRAM(uint8_t addr) {
  /* Matches your working snippet style: 0x90 then addr */
  lcdWriteIns(0x90);
  lcdWriteIns(addr);
}

static uint8_t lcd_row_start_addr(uint8_t row) {
  switch (row) {
    default:
    case 0: return (uint8_t)LCD_ROW0_ADDR;
    case 1: return (uint8_t)LCD_ROW1_ADDR;
    case 2: return (uint8_t)LCD_ROW2_ADDR;
    case 3: return (uint8_t)LCD_ROW3_ADDR;
  }
}

void lcdSetLine(uint8_t row) {
  lcdSetDDRAM(lcd_row_start_addr(row));
}

void lcdSetCursor(uint8_t row, uint8_t col) {
  lcdSetDDRAM((uint8_t)(lcd_row_start_addr(row) + col));
}

void lcdInitFromDatasheet(void) {
  /* === IS Instruction Table 0 === */
  lcdWriteIns(0x20);  /* Function Set (Table 0) */
  lcdWriteIns(0x01);  /* Clear */
  _delay_ms(20);
  lcdWriteIns(0x90);  /* Set DDRAM address (per snippet) */
  lcdWriteIns(0x00);  /* Set DDRAM address */
  lcdWriteIns(0x06);  /* Entry mode */
  lcdWriteIns(0x0C);  /* Display ON, cursor off */

  /* === IS Instruction Table 1 === */
  lcdWriteIns(0x21);  /* Function Set (Table 1) */
  lcdWriteIns(0x12);  /* Follower control / bias select */
  lcdWriteIns(0x40);  /* ICON RAM address */
  lcdWriteIns(0x30);  /* Power control 1 */
  lcdWriteIns(0x6F);  /* ICON/Power control 2 */
  lcdWriteIns(0x70);  /* Set booster / V0 control 2 */
  _delay_ms(100);

  /* === IS Instruction Table 3 === */
  lcdWriteIns(0x23);  /* Function Set (Table 3) */
  lcdWriteIns(0x81);  /* Contrast set (VOP) */
  lcdWriteIns(0x27);  /* VOP value (adjust if needed) */
  lcdWriteIns(0x82);  /* Start line setting */
  lcdWriteIns(0x00);  /* ST[5:0] */
  lcdWriteIns(0xA7);  /* Rgain set */

  /* === IS Instruction Table 2 === */
  lcdWriteIns(0x22);  /* Function Set (Table 2) */
  lcdWriteIns(0x60);  /* Display pattern (INV/AP) */
  lcdWriteIns(0x13);  /* Display mode */
  lcdWriteIns(0x44);  /* Select CGRAM & COM/SEG direction */

  /* === IS Instruction Table 3 (IST test / frame rate) === */
  lcdWriteIns(0x23);
  lcdWriteIns(0x88); lcdWriteIns(0x88); lcdWriteIns(0x88); lcdWriteIns(0x88);
  lcdWriteIns(0x28);
  lcdWriteIns(0xB2);
  lcdWriteIns(0xEF);
  lcdWriteIns(0x00);
  lcdWriteIns(0x93);
  lcdWriteIns(0x99);
  lcdWriteIns(0xE3);

  /* Back to table 0 and ensure display ON */
  lcdWriteIns(0x20);
  lcdWriteIns(0x0C);
}
