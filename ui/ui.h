#ifndef __ui_h
#define __ui_h

#include "lcd.h"
#include "lcd_port.h"

#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8u)<<8) | (((g)&0xFCu)<<3) | ((b)>>3)))

#define W 128
#define H 160

extern uint16_t fb[W * H];

void flush(void);

uint16_t blend_alpha(uint16_t bg, uint16_t fg, uint8_t alpha);
void fullblock(uint16_t x, uint16_t y, uint16_t weight, uint16_t height,
               uint16_t color, uint8_t ops);
uint16_t blend2(uint16_t a, uint16_t b);
void set_pixcolor(uint16_t *fb, uint16_t color, uint16_t x, uint16_t y);
void init_backgroundcolor(uint16_t color);



#endif