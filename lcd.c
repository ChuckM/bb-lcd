/*
 * Copyright (C) 2013 Chuck McManis (cmcmanis@mcmanis.com)
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * lcd-init.c - initialize STMF32F4 Discovery LCD expansion board
 *
 * This module configures the FSMC to be a 16 bit wide STATIC RAM
 * interface (SRAM) (sort of). The LCD controller is then controlled by writing
 * to memory in the space addresses by this interface. It should be
 * noted that most of the address lines for the FSMC port come out on
 * ports F and G which are not even available on the Butterfly board
 * because it uses the 100 pin version of the chip. 
 *
 * Basically the LCD is connected as if it were hooked to a 16 bit wide
 * parallel interface. From the FSMC it connects the following pins:
 * noting that the FSMC's Read and Write pins are on a port not available
 * on this chip (F6, F8) this code "cheats" using OE as read, and WE as
 * write.
 *
 *      OE -> LCD RD pin
 *      WE -> LCD WR pin
 *      NE1 -> LCD CS pin
 *      A19 -> LCD DC pin
 *
 * For what its worth, this makes some of the code more convoluted than
 * it should be. It also connects pin D3 (normally CLK on the FSMC) to the
 * reset line and D13 (normally A18) to the PWM/Backlight enable line.
 * 
 * The setup then just assigns to the FSMC the minimum set of lines to make
 * this board work.
 * 
 * Another random note, bus width must be 16 bits for the A19 line to work
 * the way the code expects. This has to do with whether or not A0 is for
 * byte addresses or word addresses in the FSMC.
 */

#include <stdint.h>
#include <libopencm3/cm3/assert.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/fsmc.h>
#include "lcd.h"

/* XXX: libopencm3 definitions give integer overflow error */
#define fsmc_bcr1 *((volatile uint32_t *)(0xA0000000))
#define fsmc_btr1 *((volatile uint32_t *)(0xA0000004))

void
lcd_setup() {
    uint32_t    gpio_pins;

    /* setup all the GPIO lines the LCD uses */
    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPDEN);
    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPEEN);

    /* SRAM data lines */

    /* Port D */
    gpio_pins = GPIO0 | GPIO1 | GPIO4 | GPIO5 | GPIO7 | GPIO8 | GPIO9 |
                GPIO10 | GPIO14 | GPIO15;
    gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, gpio_pins);
    gpio_set_output_options(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, gpio_pins);
    gpio_set_af(GPIOD, GPIO_AF12, gpio_pins);

    /* Port E */
    gpio_pins = GPIO3 | GPIO7 | GPIO8 | GPIO9 | GPIO10 | GPIO11 | GPIO12 |
                GPIO13 | GPIO14 | GPIO15;
    gpio_mode_setup(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, gpio_pins);
    gpio_set_output_options(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, gpio_pins);
    gpio_set_af(GPIOE, GPIO_AF12, gpio_pins);
    
    /* Nominally these are on port D (3, 13) but can be moved if needed */
    gpio_pins = LCD_RESET_PIN | LCD_PWM_PIN;
    gpio_mode_setup(LCD_CTRL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, gpio_pins);
    gpio_set_output_options(LCD_CTRL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, 
                            gpio_pins);
    

    /* Configure the NOR/SRAM region, bank 1, in the FSMC controller to 
     * pretend it has a static RAM port */
    rcc_peripheral_enable_clock(&RCC_AHB3ENR, RCC_AHB3ENR_FSMCEN);
    /* Set up FSMC_BCR1, FSMC_BTR1 */
    /* Data setup time 9 cycles, Address setup 1 cycle */
    fsmc_btr1 = FSMC_BTR_DATASTx(9) | FSMC_BTR_ADDSETx(1);
    /* Controller values 16 bits wide, Write enabled, block enabled */
    fsmc_bcr1 = (1 << 4) | FSMC_BCR_WREN | FSMC_BCR_MBKEN;
}

/*
 * What is the polarity of DC? Is level 1 "command"
 */
volatile uint16_t *__lcd_cmd_address =  (uint16_t *)(0x60000000);
volatile uint16_t *__lcd_data_address = (uint16_t *)(0x60100000);

/* Optimization state */
#ifdef RAPID_WRITE
uint16_t __next_x = 500;
uint16_t __next_y = 500;
uint16_t __last_reg_used = 0xff;
#endif

extern void msleep(int);

/* lcd_writereg()
 * Write a value to a "register" in the LCD controller, this
 * is accomplished by writing the register address first with the
 * "DC" bit reset, then writing the data with the DC bit set.
 *
 * There is an optimization though, if you are writing pixels,
 * the display auto-increments the address counters to the next 
 * pixel to write, so you don't have to resend the X_ADDR and 
 * Y_ADDR addresses.
 *
 * This allows for a bit of optimization here such that if the 
 * code is trying to write the next pixel horizontally, AND 
 * there were no intervening commands, then the write just writes
 * the pixel data register. This makes for much faster screen 
 * clears, and slighly faster character drawing.
 */
void
lcd_writereg(uint8_t addr, uint16_t val) {
#ifdef RAPID_WRITE
    if (((addr == X_RAM_ADDR) && (val == __next_x)) ||
        ((addr == Y_RAM_ADDR) && (val == __next_y)))
    {
        return;
    }
    if ((addr != RAM_DATA) || (__last_reg_used != addr))
    {
        __last_reg_used = addr;
        *(__lcd_cmd_address) = (uint16_t) addr;
    }
    *(__lcd_data_address) = val;
    if (__last_reg_used == RAM_DATA) {
        __next_x = (__next_x + 1) % LCD_DISPLAY_WIDTH;
        if (__next_x == 0) {
            __next_y = (__next_y + 1) & LCD_DISPLAY_HEIGHT;
        }
    }
#else
    *(__lcd_cmd_address) = (uint16_t) addr;
    *(__lcd_data_address) = val;
#endif
}

/* Read a 16 bit value */
uint16_t 
lcd_readreg(uint8_t addr) {
    uint16_t result;

    *(__lcd_cmd_address) = (uint16_t) addr;
#ifdef RAPID_WRITE
    __last_reg_used = addr;
#endif
    result = *(__lcd_data_address);
    return result;
}

/* 16 bit 8080 parallel interface is selected on the board */

void lcd_init() {

    /* Reset the LCD */
    gpio_clear(GPIOD, LCD_RESET_PIN);
    msleep(100); // Data sheet reccmmends waiting 100mS
    gpio_set(GPIOD, LCD_RESET_PIN);

    gpio_clear(GPIOD, GPIO13); // turn off backlight

    /* Enter sleep mode (if we are not already there).*/
    lcd_writereg(SLEEP_MODE_1, 0x0001);

    /* Set initial power parameters. */
    /* Embest does this but it seems superfluous */
    lcd_writereg(PWR_CTRL_5, 0x00B2);
  
    /* Start the oscillator.*/
    lcd_writereg(OSC_START, 0x0001);

    /* Set pixel format and basic display orientation (scanning direction).*/
    lcd_writereg(OUTPUT_CTRL, 0x30EF);
    lcd_writereg(LCD_DRIVE_AC_CTRL, 0x0600);

    /* Exit sleep mode.*/
    lcd_writereg(SLEEP_MODE_1, 0x0000);
    msleep(50);
	  
    /* Configure pixel color format and MCU interface parameters.*/
    lcd_writereg(ENTRY_MODE, 0x6830);

    /* Set analog parameters */
    lcd_writereg(SLEEP_MODE_2, 0x0999);
    lcd_writereg(ANALOG_SET, 0x3800);

    /* Enable the display */
    lcd_writereg(DISPLAY_CTRL, 0x0033);

    /* Set VCIX2 voltage to 6.1V.*/
    lcd_writereg(PWR_CTRL_2, 0x0005);

    /* Configure gamma correction.*/
    lcd_writereg(GAMMA_CTRL_1, 0x0000);
    lcd_writereg(GAMMA_CTRL_2, 0x0303);
    lcd_writereg(GAMMA_CTRL_3, 0x0407);
    lcd_writereg(GAMMA_CTRL_4, 0x0301);
    lcd_writereg(GAMMA_CTRL_5, 0x0301);
    lcd_writereg(GAMMA_CTRL_6, 0x0403);
    lcd_writereg(GAMMA_CTRL_7, 0x0707);
    lcd_writereg(GAMMA_CTRL_8, 0x0400);
    lcd_writereg(GAMMA_CTRL_9, 0x0a00);
    lcd_writereg(GAMMA_CTRL_10, 0x1000);

    /* Configure Vlcd63 and VCOMl */
    lcd_writereg(PWR_CTRL_3, 0x000A);
    lcd_writereg(PWR_CTRL_4, 0x2E00);

    /* Set the display size and ensure that the GRAM window is set to allow
       access to the full display buffer.*/
    lcd_writereg(V_RAM_POS, (LCD_DISPLAY_HEIGHT-1) << 8);
    lcd_writereg(H_RAM_START, 0x0000);
    lcd_writereg(H_RAM_END, LCD_DISPLAY_WIDTH-1);

    lcd_writereg(X_RAM_ADDR, 0x00);
    lcd_writereg(Y_RAM_ADDR, 0x00);
  
    /* clear the lcd  */
    lcd_writereg(RAM_DATA, 0x0000);
    lcd_set_background(0x00, 0xff, 0x00);
    gpio_set(GPIOD, GPIO13); // turn on backlight

    return;
}

void
lcd_set_background(int r, int g, int b) {
    uint32_t    x, y;
    uint16_t    color = pixel_rgb(r, g, b);
    for (y = 0; y < LCD_DISPLAY_HEIGHT; y++) {
        for (x = 0; x < LCD_DISPLAY_WIDTH; x++) {
            lcd_write_pixel(x, y, color);
        }
    }
}

/* simple optimization to know that the next pixel
 * written is already set. 
 */
void
lcd_write_pixel(uint16_t x, uint16_t y, uint16_t color) {
    lcd_writereg(X_RAM_ADDR, x);
    lcd_writereg(Y_RAM_ADDR, y);
    lcd_writereg(RAM_DATA, color);
}
#define LCD_PIXEL_WIDTH          320
#define LCD_PIXEL_HEIGHT         240

void
lcd_rgb_test()
{
    uint32_t index, y;
    uint16_t    grey;
    int         r, g, b;

    grey = 0;
    for (index = 0; index < LCD_DISPLAY_WIDTH; index ++) {
        r = index / 10; // 0 - 31
        grey = pixel_rgb(r << 3, r << 3, r << 3);
        for (y = 0; y < 32; y++) {
            lcd_write_pixel(index, y, grey);
        }
    }
    /* Red bar */
    for (index = 0; index < LCD_DISPLAY_WIDTH; index ++) {
        for (y = 0; y < 32; y++) {
            lcd_write_pixel(index, y+32, pixel_rgb(0xff, 0, 0));
        }
    }

    /* Green bar */
    for (index = 0; index < LCD_DISPLAY_WIDTH; index ++) {
        for (y = 0; y < 32; y++) {
            lcd_write_pixel(index, y+64, pixel_rgb(0, 0xff, 0));
        }
    }

    /* Blue bar */
    for (index = 0; index < LCD_DISPLAY_WIDTH; index ++) {
        for (y = 0; y < 32; y++) {
            lcd_write_pixel(index, y+96, pixel_rgb(0, 0, 0xff));
        }
    }

    /* technicolor bar */
    for (index = 0; index < LCD_DISPLAY_WIDTH; index ++) {
        r = index/10;
        g = (index/10 + 5) % 31;
        b = (index/10 + 10) % 31;
        grey = pixel_rgb(r << 3, g << 3, b << 3);
        for (y = 0; y < 32; y++) {
            lcd_write_pixel(index, y+128, grey);
        }
    }

    for (index = 0; index < LCD_DISPLAY_WIDTH; index ++) {
        r = (index/10 + 2) % 31;
        g = (index/10 + 8) % 31;
        b = (index/10 + 16) % 31;
        grey = pixel_rgb(r << 3, g << 3, b << 3);
        for (y = 0; y < 32; y++) {
            lcd_write_pixel(index, y+160, grey);
        }
    }

    for (index = 0; index < LCD_DISPLAY_WIDTH; index ++) {
        r = (index/10 + 3) % 31;
        g = (index/10 + 7) % 31;
        b = (index/10 + 5) % 31;
        grey = pixel_rgb(r << 3, g << 3, b << 3);
        for (y = 0; y < 32; y++) {
            lcd_write_pixel(index, y+192, grey);
        }
    }
	  
    /* Border */
    for (index = 0; index < 320; index++) {
        lcd_write_pixel(index, 0, 0xffff);
        lcd_write_pixel(index, 239, 0xffff);
    }
    for (index = 0; index < 240; index++) {
        lcd_write_pixel(0, index, 0xffff);
        lcd_write_pixel(319, index, 0xffff);
    }
    
}


