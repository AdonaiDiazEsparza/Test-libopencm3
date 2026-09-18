#include "minimal_oled.h"
#include "fonts.h"
#include <string.h>
#include <stdbool.h>

// oled definitions
#define OLED_ADDR         0x3C    // 7-bit I2C address (libopencm3 shifts it)
#define OLED_CMD_MODE     0x00    // set command mode
#define OLED_DAT_MODE     0x40    // set data mode

// oled commands
#define OLED_COLUMN_LOW   0x00    // set lower 4 bits of start column (0x00 - 0x0F)
#define OLED_COLUMN_HIGH  0x10    // set higher 4 bits of start column (0x10 - 0x1F)
#define OLED_MEMORYMODE   0x20    // set memory addressing mode (following byte)
#define OLED_COLUMNS      0x21    // set start and end column (following 2 bytes)
#define OLED_PAGES        0x22    // set start and end page (following 2 bytes)
#define OLED_SCROLL_OFF   0x2E    // deactivate scroll command
#define OLED_STARTLINE    0x40    // set display start line (0x40-0x7F = 0-63)
#define OLED_CONTRAST     0x81    // set display contrast (following byte)
#define OLED_CHARGEPUMP   0x8D    // (following byte - 0x14:enable, 0x10: disable)
#define OLED_XFLIP_OFF    0xA0    // don't flip display horizontally
#define OLED_XFLIP        0xA1    // flip display horizontally
#define OLED_INVERT_OFF   0xA6    // set non-inverted display
#define OLED_INVERT       0xA7    // set inverse display
#define OLED_MULTIPLEX    0xA8    // set multiplex ratio (following byte)
#define OLED_DISPLAY_OFF  0xAE    // set display off (sleep mode)
#define OLED_DISPLAY_ON   0xAF    // set display on
#define OLED_PAGE         0xB0    // set start page (following byte)
#define OLED_YFLIP_OFF    0xC0    // don't flip display vertically
#define OLED_YFLIP        0xC8    // flip display vertically
#define OLED_OFFSET       0xD3    // set display offset (y-scroll: following byte)
#define OLED_COMPINS      0xDA    // set COM pin config (following byte)
#define OLED_CLKDIV       0xD5    // set display clock divide ratio / oscillator frequency
#define OLED_VCOMH        0xDB    // set VCOMH deselect level
#define OLED_MEMORY_PAGE  0x02    // page addressing mode
#define OLED_MEMORY_HORI  0x00    // horizontal addressing mode


#define OLED_WIDTH         128

#ifdef CONFIG_RESOLUTION_128X64

#define OLED_HEIGHT 64

static const uint8_t SSD1306_128X64_INIT_CMD[] = {
  OLED_CMD_MODE,
  OLED_MULTIPLEX,   0x3F,                 // set multiplex ratio  
  OLED_CHARGEPUMP,  0x14,                 // set DC-DC enable  
  OLED_MEMORYMODE,  0x00,                 // set horizontal addressing mode
  OLED_COLUMNS,     0x00, 0x7F,           // set start and end column
  OLED_PAGES,       0x00, 0x3F,           // set start and end page
  OLED_COMPINS,     0x12,                 // set com pins
  OLED_XFLIP, OLED_YFLIP,                 // flip screen
  OLED_DISPLAY_ON                         // display on
};

#else

#define OLED_HEIGHT 32

static const uint8_t SSD1306_128X32_INIT_CMD[] = {
  OLED_CMD_MODE,
  OLED_MULTIPLEX,   0x1F,                 // set multiplex ratio  
  OLED_CHARGEPUMP,  0x14,                 // set DC-DC enable  
  OLED_MEMORYMODE,  0x00,                 // set horizontal addressing mode
  OLED_COLUMNS,     0x00, 0x7F,           // set start and end column
  OLED_PAGES,       0x00, 0x1F,           // set start and end page
  OLED_COMPINS,     0x02,                 // set com pins
  OLED_XFLIP, OLED_YFLIP,                 // flip screen
  OLED_DISPLAY_ON                         // display on
};

#endif

#define OLED_NUM_PAGES (OLED_HEIGHT / 8)

static uint8_t oled_buf[OLED_NUM_PAGES][OLED_WIDTH];

static uint32_t oled_i2c;

/*
 * I2C pins used on STM32F411CEU6 (Blackpill):
 *   I2C1: PB8 = SCL, PB9 = SDA  (AF4)
 *   I2C2: PB10 = SCL (AF4), PB3 = SDA (AF9)
 *   I2C3: PA8 = SCL (AF4), PB4 = SDA (AF9)
 */
static void oled_i2c_gpio_setup(uint32_t port, uint16_t pins, uint8_t af)
{
    gpio_mode_setup(port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, pins);
    gpio_set_output_options(port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, pins);
    gpio_set_af(port, af, pins);
}

/*
* Function to transmit data to the oled display
* @param data: pointer to the data to transmit
* @param length: length of the data to transmit
*/
static void oled_i2c_transmit(const uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return;
    }

    i2c_transfer7(oled_i2c, OLED_ADDR, data, length, NULL, 0);
}

/*
* Function to initialize the oled
* @param i2c: i2c peripheral to use
*/
void oled_init(uint32_t i2c)
{
    enum rcc_periph_clken i2c_clk;
    enum rcc_periph_rst i2c_rst;

    switch (i2c) {
    case I2C1:
        i2c_clk = RCC_I2C1;
        i2c_rst = RST_I2C1;
        rcc_periph_clock_enable(RCC_GPIOB);
        oled_i2c_gpio_setup(GPIOB, GPIO8 | GPIO9, GPIO_AF4);
        break;
    case I2C2:
        i2c_clk = RCC_I2C2;
        i2c_rst = RST_I2C2;
        rcc_periph_clock_enable(RCC_GPIOB);
        oled_i2c_gpio_setup(GPIOB, GPIO10, GPIO_AF4);
        oled_i2c_gpio_setup(GPIOB, GPIO3, GPIO_AF9);
        break;
#ifdef I2C3
    case I2C3:
        i2c_clk = RCC_I2C3;
        i2c_rst = RST_I2C3;
        rcc_periph_clock_enable(RCC_GPIOA);
        rcc_periph_clock_enable(RCC_GPIOB);
        oled_i2c_gpio_setup(GPIOA, GPIO8, GPIO_AF4);
        oled_i2c_gpio_setup(GPIOB, GPIO4, GPIO_AF9);
        break;
#endif
    default:
        return;
    }

    rcc_periph_clock_enable(i2c_clk);
    rcc_periph_reset_pulse(i2c_rst);

    i2c_peripheral_disable(i2c);
    i2c_set_speed(i2c, i2c_speed_sm_100k, rcc_apb1_frequency / 1000000);
    i2c_peripheral_enable(i2c);

    oled_i2c = i2c;

    #ifdef CONFIG_RESOLUTION_128X64
    oled_i2c_transmit(SSD1306_128X64_INIT_CMD, sizeof(SSD1306_128X64_INIT_CMD));
    #else
    oled_i2c_transmit(SSD1306_128X32_INIT_CMD, sizeof(SSD1306_128X32_INIT_CMD));
    #endif
}

/*
* Function to flush a page of the oled buffer
* @param page: page to flush
*/
void oled_flush_page(uint8_t page)
{
    uint8_t cmd[] = {
        OLED_CMD_MODE,
        OLED_COLUMNS, 0x00, OLED_WIDTH - 1,
        OLED_PAGES, page, page
    };
    uint8_t data[OLED_WIDTH + 1];

    if (page >= OLED_NUM_PAGES) {
        return;
    }

    oled_i2c_transmit(cmd, sizeof(cmd));

    data[0] = OLED_DAT_MODE;
    memcpy(&data[1], oled_buf[page], OLED_WIDTH);
    oled_i2c_transmit(data, sizeof(data));
}

/*
* Function to flush the oled buffer
*/
void oled_flush(void)
{
  for (uint8_t p = 0; p < OLED_NUM_PAGES; p++)
  {
    oled_flush_page(p);
  }
}

/*
* Function to clear the oled buffer
*/
void oled_clear_buffer(void)
{
  memset(oled_buf, 0x00, sizeof(oled_buf));
}

/**
 * @fn oled_clear
 * 
 * @brief Clear the oled and display
 * 
 * @param none
 */
void oled_clear(void)
{
   oled_clear_buffer();
   oled_flush();
}


/*
* Function to set a pixel on the oled
* @param x: x position of the pixel
* @param y: y position of the pixel
* @param color: color of the pixel
*/
void oled_set_pixel(int16_t x, int16_t y, bool color)
{
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
    return;
  uint8_t page = y >> 3;  // y / 8
  uint8_t bit = y & 0x07; // y % 8
  if (color)
    oled_buf[page][x] |= (1 << bit);
  else
    oled_buf[page][x] &= ~(1 << bit);
}

/*
* Function to draw a bitmap on the oled
* @param x: x position of the bitmap
* @param y: y position of the bitmap
* @param w: width of the bitmap
* @param h: height of the bitmap
* @param bitmap: pointer to the bitmap
*/
void oled_draw_bmp(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bitmap)
{
  for (int16_t j = 0; j < h; j++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      int byte_index = (j / 8) * w + i;
      uint8_t bit_mask = 1 << (j & 7);

      uint8_t pixel = (bitmap[byte_index] & bit_mask) ? 1 : 0;

      oled_set_pixel(x + i, y + j, pixel);
    }
  }
}

/**
 * @fn transpose_letter
 * 
 * @brief transpose the letter to match with the oled
 * 
 * @param in array to transpose
 * @param out array transposed
 * @param w width of the letter
 * @param h height of the letter
 */
static void transpose_letter(const char in[8], uint8_t out[8],uint8_t width, uint8_t height)
{
  for (uint8_t x = 0; x < width; x++)
  {
    uint8_t col = 0;
    for (uint8_t y = 0; y < height; y++)
    {
      if (in[y] & (1 << x))
      {
        col |= (1 << y);
      }
    }
    out[x] = col;
  }
}

/**
 * @fn oled_draw_char8x8
 * 
 * @brief draw a single char for font 8x8
 * 
 * @param x set position of the letter on x
 * @param y set position of the letter on y
 * @param c character to display on buffer
 */
void oled_draw_char8x8(uint8_t x, uint8_t y, char c)
{
  if (c < 32 || c > 126)
    c = ' ';
  uint8_t transposed[8];
  transpose_letter(font8x8_basic[c - 32], transposed,8,8);
  oled_draw_bmp(x, y,8, 8, transposed);
}

/**
 * @fn oled_print_8x8
 * 
 * @brief draw a text on the oled with font 8x8
 * 
 * @param x set position of the text on x
 * @param y set position of the text on y
 * @param text  text to display on the oled
 */
void oled_print_8x8(uint8_t x, uint8_t y, const char *text)
{
  while (*text)
  {
    oled_draw_char8x8(x, y, *text++);
    x += 8;
  }
}

/**
 * @fn oled_draw_char6x8
 * 
 * @brief draw a single char for font 6x8
 * 
 * @param x set position of the letter on x
 * @param y set position of the letter on y
 * @param c character to display on buffer
 */
void oled_draw_char6x8(uint8_t x, uint8_t y, char c)
{
  if (c < 32 || c > 126)
    c = ' ';
  oled_draw_bmp(x, y, 6, 8, (const uint8_t *)font_6x8[c - 32]);
}

/**
 * @fn oled_print_6x8
 * 
 * @brief draw a text on the oled with font 6x8
 * 
 * @param x set position of the text on x
 * @param y set position of the text on y
 * @param text  text to display on the oled
 */
void oled_print_6x8(uint8_t x, uint8_t y, const char *text)
{
  while (*text)
  {
    oled_draw_char6x8(x, y, *text++);
    x += 7;
  }
}

/**
 * @fn oled_draw_char5x8
 * 
 * @brief draw a single char for font 5x8
 * 
 * @param x set position of the letter on x
 * @param y set position of the letter on y
 * @param c character to display on buffer
 */
void oled_draw_char5x8(uint8_t x, uint8_t y, char c)
{
  if (c < 32 || c > 126)
    c = ' ';
  oled_draw_bmp(x, y, 5, 8, (const uint8_t *)font_5x8[c - 32]);
}

/**
 * @fn oled_print_5x8
 * 
 * @brief draw a text on the oled with font 5x8
 * 
 * @param x set position of the text on x
 * @param y set position of the text on y
 * @param text  text to display on the oled
 */
void oled_print_5x8(uint8_t x, uint8_t y, const char *text)
{
  while (*text)
  {
    oled_draw_char5x8(x, y, *text++);
    x += 6;
  }
}

/**
 * @fn oled_draw_hline
 *
 * @brief Draw a horizontal line
 *
 * @param x starting x position
 * @param y position on y axis
 * @param length size of the line
 * @param color 0 = off, 1 = on
 */
void oled_draw_hline(uint8_t x, uint8_t y, uint8_t length, bool color)
{
    if (y >= OLED_HEIGHT || x >= OLED_WIDTH) {
        return;
    }

    if (x + length > OLED_WIDTH) {
        length = OLED_WIDTH - x;
    }

    if (length == 0) return;

    for (uint8_t i = 0; i < length; i++)
    {
        oled_set_pixel(x + i, y, color);
    }
}

/**
 * @fn oled_draw_vline
 *
 * @brief Draw a vertical line
 *
 * @param x position on x axis
 * @param y starting y position
 * @param length size of the line
 * @param color 0 = off, 1 = on
 */
void oled_draw_vline(uint8_t x, uint8_t y, uint8_t length, bool color)
{
    if (x >= OLED_WIDTH) return;
    
    if (y + length > OLED_HEIGHT) {
        length = OLED_HEIGHT - y;
    }
    if (length == 0) return;

    for (uint8_t i = 0; i < length; i++)
    {
        oled_set_pixel(x, y + i, color);
    }
}

/**
 * @fn oled_draw_rect
 *
 * @brief Draw an outlined rectangle
 *
 * @param x starting x position
 * @param y starting y position
 * @param width rectangle width
 * @param height rectangle height
 * @param color 0 = off, 1 = on
 */
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool color)
{
    if (width == 0 || height == 0) {
        return;
    }

    // Top
    oled_draw_hline(x, y, width, color);

    // Bottom
    oled_draw_hline(x, y + height - 1, width, color);

    // Left
    oled_draw_vline(x, y, height, color);

    // Right
    oled_draw_vline(x + width - 1, y, height, color);
}