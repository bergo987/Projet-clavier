/**
 * ssd1306.h — Lightweight SSD1306 OLED driver for Raspberry Pi Pico (C SDK)
 *
 * Target display : SparkFun Micro OLED Breakout LCD-22495 (64×48 px, I2C)
 * Controller     : Solomon Systech SSD1306
 * Interface      : I2C (400 kHz fast mode)
 *
 * Usage
 * -----
 *   1. #include "ssd1306.h" in exactly ONE .c file (define SSD1306_IMPL first):
 *
 *        #define SSD1306_IMPL
 *        #include "ssd1306.h"
 *
 *   2. In every other file that needs the API, just #include "ssd1306.h"
 *      (without the #define).
 *
 * Quick-start
 * -----------
 *   ssd1306_init();
 *   ssd1306_clear();
 *   ssd1306_draw_string(0, 0, "Current:");
 *   ssd1306_draw_string(0, 2, "1.23 A");
 *   ssd1306_flush();
 *
 * Pin defaults (change the #defines below if needed)
 * ---------------------------------------------------
 *   SDA → GP4  (Pico pin 6)
 *   SCL → GP5  (Pico pin 7)
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* ── Configuration ────────────────────────────────────────────────────────── */

#ifndef SSD1306_I2C_PORT
#define SSD1306_I2C_PORT    i2c0
#endif

#ifndef SSD1306_SDA_PIN
#define SSD1306_SDA_PIN     8
#endif

#ifndef SSD1306_SCL_PIN
#define SSD1306_SCL_PIN     9
#endif

#ifndef SSD1306_I2C_ADDR
#define SSD1306_I2C_ADDR    0x3D   /* 0x3D if SA0 jumper is set */
#endif

    /* SparkFun LCD-22495 is 64×48. Change if you use a different screen. */
#define SSD1306_WIDTH       64
#define SSD1306_HEIGHT      48
#define SSD1306_PAGES       (SSD1306_HEIGHT / 8)   /* 6 pages */

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * Initialise I2C and the display.
 * Must be called once before any other function.
 */
void ssd1306_init(void);

/**
 * Push the internal framebuffer to the display.
 * Nothing becomes visible until you call this.
 */
void ssd1306_flush(void);

/** Fill the entire framebuffer with 0 (all pixels off). */
void ssd1306_clear(void);

/** Fill the entire framebuffer with 0xFF (all pixels on). */
void ssd1306_fill(void);

/**
 * Set or clear a single pixel.
 * x : 0 … SSD1306_WIDTH-1
 * y : 0 … SSD1306_HEIGHT-1
 */
void ssd1306_draw_pixel(int x, int y, bool on);

/**
 * Draw a horizontal line from (x0,y) to (x1,y).
 */
void ssd1306_draw_hline(int x0, int x1, int y, bool on);

/**
 * Draw a vertical line from (x,y0) to (x,y1).
 */
void ssd1306_draw_vline(int x, int y0, int y1, bool on);

/**
 * Draw an unfilled rectangle.
 */
void ssd1306_draw_rect(int x, int y, int w, int h, bool on);

/**
 * Draw a filled rectangle.
 */
void ssd1306_fill_rect(int x, int y, int w, int h, bool on);

/**
 * Draw a Bresenham line from (x0,y0) to (x1,y1).
 */
void ssd1306_draw_line(int x0, int y0, int x1, int y1, bool on);

/**
 * Draw a single ASCII character using the built-in 5×7 font.
 *
 * col  : character column, 0-based  (each char is 6 px wide including gap)
 * page : character row,    0-based  (each char is 8 px tall = 1 page)
 *
 * The display fits: 10 columns × 6 rows  (64 / 6 = 10,  48 / 8 = 6)
 */
void ssd1306_draw_char(int col, int page, char c);

/**
 * Draw a null-terminated string.
 * Wraps automatically when reaching the right edge.
 *
 * col  : starting character column
 * page : starting page (row)
 */
void ssd1306_draw_string(int col, int page, const char *str);

/**
 * Draw a large 2×-scaled character (12×16 px, 2 pages tall).
 * Fits 5 large chars per row.
 */
void ssd1306_draw_char_large(int col, int page, char c);

/**
 * Draw a large 2×-scaled string.
 */
void ssd1306_draw_string_large(int col, int page, const char *str);

/**
 * Draw a simple bar-graph (useful for showing current magnitude).
 *
 * x, y : top-left corner of the bar area
 * w    : total width of the bar area in pixels
 * h    : height of the bar in pixels
 * fill : fraction to fill, 0.0 … 1.0
 */
void ssd1306_draw_bar(int x, int y, int w, int h, float fill);

/**
 * Turn the display on or off without changing VRAM contents.
 */
void ssd1306_power(bool on);

/**
 * Invert all pixels on the physical display (does not touch the framebuffer).
 */
void ssd1306_invert(bool invert);

/* ── Implementation (compiled in exactly one translation unit) ────────────── */

#ifdef SSD1306_IMPL

/* ── Internal framebuffer ─────────────────────────────────────────────────── */
static uint8_t _fb[SSD1306_WIDTH * SSD1306_PAGES];

/* ── 5×7 font (ASCII 0x20–0x7E, one entry = 5 column bytes) ──────────────── */
static const uint8_t _font[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' 0x20 */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x14,0x08,0x3E,0x08,0x14}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */
    {0x10,0x08,0x08,0x10,0x08}, /* '~' 0x7E */
};

/* ── Low-level helpers ────────────────────────────────────────────────────── */

static void _cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};  /* 0x00 = Co=0, D/C=0 → command */
    i2c_write_blocking(SSD1306_I2C_PORT, SSD1306_I2C_ADDR, buf, 2, false);
}

/* ── Public API implementation ────────────────────────────────────────────── */

void ssd1306_init(void) {
    i2c_init(SSD1306_I2C_PORT, 400 * 1000);
    gpio_set_function(SSD1306_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_SDA_PIN);
    gpio_pull_up(SSD1306_SCL_PIN);
    sleep_ms(100);  /* let the display power up */

    /* SSD1306 initialisation sequence tuned for 64×48 */
    _cmd(0xAE);         /* display OFF */

    _cmd(0xD5);         /* set display clock divide ratio */
    _cmd(0x80);

    _cmd(0xA8);         /* set multiplex ratio */
    _cmd(SSD1306_HEIGHT - 1);   /* 47 for 48 rows */

    _cmd(0xD3);         /* display offset */
    _cmd(0x00);

    _cmd(0x40);         /* display start line = 0 */

    _cmd(0x8D);         /* charge pump */
    _cmd(0x14);         /* enable */

    _cmd(0x20);         /* memory addressing mode */
    _cmd(0x00);         /* horizontal */

    _cmd(0xA1);         /* segment re-map: col 127 → SEG0 */
    _cmd(0xC8);         /* COM scan direction: remapped */

    _cmd(0xDA);         /* COM pins hardware config */
    _cmd(0x12);         /* alternative COM, no left/right remap */

    _cmd(0x81);         /* contrast */
    _cmd(0xCF);

    _cmd(0xD9);         /* pre-charge period */
    _cmd(0xF1);

    _cmd(0xDB);         /* VCOMH deselect level */
    _cmd(0x40);

    _cmd(0xA4);         /* entire display ON → follow RAM */
    _cmd(0xA6);         /* normal display (not inverted) */

    _cmd(0x2E);         /* deactivate scroll */

    /* Constrain column window to the 64 active columns.
       On some SSD1306 variants the physical panel is 128 wide
       but only 64 columns are wired — this centres it correctly. */
    /* Constrain column window to the 64 active columns. */
    _cmd(0x21);           /* set column address */
    _cmd(32);             /* Start à 32 (Offset pour SparkFun) */
    _cmd(32 + 64 - 1);    /* End à 95 */

    _cmd(0x22);           /* set page address */
    _cmd(0);              /* start = page 0 */
    _cmd(SSD1306_PAGES - 1);

    memset(_fb, 0, sizeof(_fb));
    ssd1306_flush();

    _cmd(0xAF);         /* display ON */
}

void ssd1306_flush(void) {
    /* On définit la fenêtre d'écriture AVEC l'offset de 32 */
    _cmd(0x21); 
    _cmd(32);                             /* Start Column 32 */
    _cmd(32 + SSD1306_WIDTH - 1);         /* End Column 95 */
    
    _cmd(0x22); 
    _cmd(0x00); 
    _cmd(SSD1306_PAGES - 1);

    /* Envoi du framebuffer */
    uint8_t tmp[SSD1306_WIDTH + 1];
    tmp[0] = 0x40;
    for (int p = 0; p < SSD1306_PAGES; p++) {
        memcpy(&tmp[1], &_fb[p * SSD1306_WIDTH], SSD1306_WIDTH);
        i2c_write_blocking(SSD1306_I2C_PORT, SSD1306_I2C_ADDR, tmp, SSD1306_WIDTH + 1, false);
    }
}

void ssd1306_clear(void) {
    memset(_fb, 0x00, sizeof(_fb));
}

void ssd1306_fill(void) {
    memset(_fb, 0xFF, sizeof(_fb));
}

void ssd1306_power(bool on) {
    _cmd(on ? 0xAF : 0xAE);
}

void ssd1306_invert(bool invert) {
    _cmd(invert ? 0xA7 : 0xA6);
}

void ssd1306_draw_pixel(int x, int y, bool on) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    int page = y / 8;
    int bit  = y % 8;
    if (on)
        _fb[page * SSD1306_WIDTH + x] |=  (1 << bit);
    else
        _fb[page * SSD1306_WIDTH + x] &= ~(1 << bit);
}

void ssd1306_draw_hline(int x0, int x1, int y, bool on) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (int x = x0; x <= x1; x++) ssd1306_draw_pixel(x, y, on);
}

void ssd1306_draw_vline(int x, int y0, int y1, bool on) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) ssd1306_draw_pixel(x, y, on);
}

void ssd1306_draw_rect(int x, int y, int w, int h, bool on) {
    ssd1306_draw_hline(x,       x + w - 1, y,           on);
    ssd1306_draw_hline(x,       x + w - 1, y + h - 1,   on);
    ssd1306_draw_vline(x,       y,         y + h - 1,   on);
    ssd1306_draw_vline(x + w - 1, y,       y + h - 1,   on);
}

void ssd1306_fill_rect(int x, int y, int w, int h, bool on) {
    for (int row = y; row < y + h; row++)
        ssd1306_draw_hline(x, x + w - 1, row, on);
}

void ssd1306_draw_line(int x0, int y0, int x1, int y1, bool on) {
    /* Bresenham's line algorithm */
    int dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        ssd1306_draw_pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ssd1306_draw_char(int col, int page, char c) {
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *glyph = _font[(uint8_t)(c - 0x20)];
    int x = col * 6;  /* 5 px glyph + 1 px gap */
    if (x + 5 > SSD1306_WIDTH || page >= SSD1306_PAGES) return;
    for (int i = 0; i < 5; i++)
        _fb[page * SSD1306_WIDTH + x + i] = glyph[i];
    if (x + 5 < SSD1306_WIDTH)
        _fb[page * SSD1306_WIDTH + x + 5] = 0x00; /* gap */
}

void ssd1306_draw_string(int col, int page, const char *str) {
    int max_cols = SSD1306_WIDTH / 6;  /* 10 for 64 px wide */
    while (*str) {
        ssd1306_draw_char(col, page, *str++);
        if (++col >= max_cols) { col = 0; page++; }
        if (page >= SSD1306_PAGES) break;
    }
}

void ssd1306_draw_char_large(int col, int page, char c) {
    /* 2× scale: each original pixel becomes a 2×2 block,
       so each char occupies 10 px wide × 2 pages tall. */
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *glyph = _font[(uint8_t)(c - 0x20)];
    int x = col * 12;   /* 10 px glyph + 2 px gap */

    for (int i = 0; i < 5; i++) {
        uint8_t col_data = glyph[i];
        /* Expand 8 bits → 16 bits (each bit duplicated) */
        uint16_t expanded = 0;
        for (int b = 0; b < 8; b++) {
            if (col_data & (1 << b)) {
                expanded |= (3 << (b * 2));
            }
        }
        /* Write into two pages, two x-columns per original column */
        for (int dx = 0; dx < 2; dx++) {
            int px = x + i * 2 + dx;
            if (px >= SSD1306_WIDTH) continue;
            /* lower page  */
            if (page     < SSD1306_PAGES)
                _fb[page       * SSD1306_WIDTH + px] = (uint8_t)(expanded & 0xFF);
            /* upper page */
            if (page + 1 < SSD1306_PAGES)
                _fb[(page + 1) * SSD1306_WIDTH + px] = (uint8_t)(expanded >> 8);
        }
    }
}

void ssd1306_draw_string_large(int col, int page, const char *str) {
    int max_cols = SSD1306_WIDTH / 12;  /* 5 large chars across 64 px */
    while (*str) {
        ssd1306_draw_char_large(col, page, *str++);
        if (++col >= max_cols) { col = 0; page += 2; }
        if (page + 1 >= SSD1306_PAGES) break;
    }
}

void ssd1306_draw_bar(int x, int y, int w, int h, float fill) {
    if (fill < 0.0f) fill = 0.0f;
    if (fill > 1.0f) fill = 1.0f;
    /* Outer border */
    ssd1306_draw_rect(x, y, w, h, true);
    /* Inner fill */
    int inner_w = (int)((w - 2) * fill);
    if (inner_w > 0)
        ssd1306_fill_rect(x + 1, y + 1, inner_w, h - 2, true);
    /* Clear unfilled portion */
    int gap = w - 2 - inner_w;
    if (gap > 0)
        ssd1306_fill_rect(x + 1 + inner_w, y + 1, gap, h - 2, false);
}

#endif /* SSD1306_IMPL */
#endif /* SSD1306_H */