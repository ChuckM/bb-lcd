/* LCD functions */

#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include "lcd.h"
#include "util.h"
#include "gfx.h"

int configure_fsmc(char *, int);

volatile uint16_t *test_ptr = (uint16_t *)(0x60000000);
volatile uint16_t dummy_data;

int show_time(void);
void fill_box(uint16_t, uint16_t, int);

/*
 * show_time()
 *
 * Draw a rounded rectangle with the 'time' in it. My SysTick
 * timer pumps the variable system_millis once per millisecond
 * this code takes that, and create a 'time' out of it in the
 * form of an elapsed time counter for hours, minutes, seconds
 * and fractions of a second. 
 */
int
show_time() {
    uint32_t i;
    uint32_t t;
    int res;

    gfx_setTextSize(2);
    // hh:mm:ss.mmm
    // 0....5....a. 
    char timestring[13];

    t = mtime();
    i = t % 1000;
    t = t / 1000;
    timestring[12] = '\000';
    timestring[11] = (char)(i % 10) + '0';
    timestring[10] = (char)((i/10) % 10) + '0';
    timestring[9] = (char)((i/100) % 10) + '0';
    timestring[8] = '.';
    i = t % 60;
    // every 10 sec we tell the world 10 sec has passed
    res = ((i % 10) == 0) ? 1 : 0;
    t = t / 60;
    timestring[7] = (char)(i % 10) + '0';
    timestring[6] = (char)((i/10) % 10) + '0';
    timestring[5] = ':';
    i = t % 60;
    t = t / 60;
    timestring[4] = (char)(i % 10) + '0';
    timestring[3] = (char)((i/10) % 10) + '0';
    timestring[2] = ':';
    i = t % 24;
    timestring[1] = (char)(i % 10) + '0';
    timestring[0] = (char)((i/10) % 10) + '0';
    gfx_fillRoundRect(40, 190, 240, 32, 15, GFX_COLOR_BLUE);
    gfx_drawRoundRect(40, 190, 240, 32, 15, GFX_COLOR_WHITE);
    gfx_setCursor(40 + (240 - 16*12)/2, 190 + (32 - 18)/2);
    gfx_setTextColor(GFX_COLOR_YELLOW, GFX_COLOR_YELLOW);
    gfx_puts(timestring);
    return res;
}

/*
 * fill_box()
 *
 * This draws a box filled with a color scheme and labels it
 * the choices are red, green, blue, pallete.
 */
void
fill_box(uint16_t x, uint16_t y, int fill_type) {
    char *label;
    uint16_t color;
    uint16_t r, g, b;
    uint16_t dx, dy;
    int xwid;

    gfx_setTextColor(GFX_COLOR_WHITE, GFX_COLOR_WHITE);

    switch (fill_type) {
        case 0:
            color = GFX_COLOR_RED;
            label = "RED";
            xwid = 16*3;
            break;
        case 1:
            color = GFX_COLOR_GREEN;
            label = "GREEN";
            xwid = 16*5;
            break;
        case 2:
            color = GFX_COLOR_BLUE;
            label = "BLUE";
            xwid = 16*4;
            break;
        default:
            color = GFX_COLOR_WHITE;
            label = "MULTI";
            gfx_setTextColor(GFX_COLOR_BLACK, GFX_COLOR_BLACK);
            xwid = 16*5;
            break;
    }
    
    gfx_fillRoundRect(x, y, 100, 60, 10, color);
    if (fill_type > 2) {
        for (dx = 0; dx < 100; dx++) {
            for (dy = 0; dy < 60; dy++) {
                b = (dy < 30) ? (255 * dy/30) : 255;
                r = (dx < 50) ? (255 * dx/50) : 255;
                g = (dx > 50) ? (255 * (100 - dx)) : 255;
                color = pixel_rgb(r, g, b);
                gfx_drawPixel(x+dx, y+dy, color);
            }
        }
    }
    gfx_drawRoundRect(x, y, 100, 60, 10, GFX_COLOR_WHITE);
    gfx_setTextSize(2);
    gfx_setCursor(x+(100 - xwid)/2, y+(60 - 18)/2);
    gfx_puts(label);
}

void show_grey(void);

/*
 * show_grey 
 *
 * This puts a grey scale strip on the LCD for some dynamic range
 * assesment.
 */
void
show_grey() {
    uint16_t y;
    uint16_t g;
    gfx_fillRect(135, 35, 50, 135, GFX_COLOR_BLACK);
    for (y = 0; y < 135; y++) {
        uint16_t color;
        g = (y * 255) / 135;
        color = pixel_rgb(g,g,g);
        gfx_drawFastHLine(135, y+35, 50, color);
    }
    gfx_drawRect(135, 35, 50, 135, GFX_COLOR_WHITE);
}

int
main(void) {
    /* configure FSMC for bank 4 static RAM */
    uint16_t toggle = 0;

    clock_setup();
    systick_setup();
    uart_setup(115200);

    uart_puts("\nMy LCD Demo 0.1\n");

    uart_puts("Configuring fsmc...\n");
    lcd_setup();
    uart_puts("Done .\n");
    uart_puts("LCD Init ...\n");
    lcd_init();
    gfx_init();
    gfx_setTextColor(GFX_COLOR_BLACK, GFX_COLOR_BLACK);
    gfx_setTextSize(2);
    gfx_setCursor(10, 10);
    gfx_puts("LCD Test demo\nTesting lower case.\n switching in 5 seconds");
    uart_puts("Done [LCD Init]\n");
    uart_puts(" Next step in 5 seconds ... Screen should GREEN\n");
    msleep(5000);
    lcd_rgb_test();
    uart_puts(" Next step in 5 seconds, screen should be MULTICOLORED\n");
    msleep(5000);

    gfx_setTextColor(GFX_COLOR_WHITE, GFX_COLOR_WHITE);
    lcd_set_background(0x0, 0, 0x0);
    gfx_setTextSize(2);
    gfx_setCursor((320 - 17*16)/2, 0);
    gfx_puts("LCD Demonstration");

/* box is 100 x 60, screen is 320 wide, so in the left side is 
 * x=30, y = 15, y= 80, and the right side, x = 190, y = 15, y = 80
 */
    while (1) {
        int blip = 0;
        fill_box(20, 35, toggle & 0x3);
        fill_box(200, 35, (toggle + 1) & 0x3);
        fill_box(200, 110, (toggle + 2) & 0x3);
        fill_box(20, 110, (toggle + 3) & 0x3);
        show_grey();
        uart_puts("Type space to continue ...\n");
        while ((uart_getc(0) != ' ') && ! blip) {
            blip = show_time();
            msleep(100);
        }
        toggle++;
    }
}

