#pragma once

#include <stdint.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>

/*
* This library is made only for the 128x64 oled display and the 128x32 oled display
* for the controller SSD1306
*/

#ifdef __cplusplus
extern "C" {
#endif

void oled_init(uint32_t i2c);
void oled_flush_page(uint8_t page);
void oled_flush(void);
void oled_clear_buffer(void);
void oled_clear(void);
void oled_set_pixel(int16_t x, int16_t y, bool color);
void oled_draw_bmp(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bitmap);
void oled_draw_char8x8(uint8_t x, uint8_t y, char c);
void oled_print_8x8(uint8_t x, uint8_t y, const char *text);
void oled_draw_char6x8(uint8_t x, uint8_t y, char c);
void oled_print_6x8(uint8_t x, uint8_t y, const char *text);
void oled_draw_char5x8(uint8_t x, uint8_t y, char c);
void oled_print_5x8(uint8_t x, uint8_t y, const char *text);
void oled_draw_hline(uint8_t x, uint8_t y, uint8_t length, bool color);
void oled_draw_vline(uint8_t x, uint8_t y, uint8_t length, bool color);
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool color);


#ifdef __cplusplus
}
#endif