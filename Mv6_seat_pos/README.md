# lcd_mccog42005a6w - Kurz-Dokumentation

Für das Display kann die "lcd_mccog42005a6w" Library genutzt werden.

Die Library besteht aus 4 main Funktionen:
lcdBegin(); //initalisiert das display und ggf. i2C-Bus
lcdSetCursor(uint8_t row, uint8_t col) //Setzt den Cursor auf Zeile/Spalte
lcdWriteText(const char *s) //String ab der aktuellen Cursor-Position.
lcdWriteTextf(const char *fmt, ...) //`printf`-aehnliche Ausgabe auf das LCD.


## Komplettes Minimalbeispiel
```c
#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include "lcd_mccog42005a6w.h"

int main(void) {
  lcdBegin();

  lcdSetCursor(0, 0);
  lcdWriteText("Hello World!");

  uint16_t counter = 0;
  while (1) {
    lcdSetCursor(1, 0);
    lcdWriteTextf("Counter: %-12u", counter++);
    _delay_ms(500);
  }
}
```

