/*
 * Copyright (C) 2013 Chuck McManis (cmcmanis@mcmanis.com)
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Include file - with prototype for lcd init
 */
#ifndef LCD_H
#define LCD_H
void lcd_setup(void);
void lcd_init(void);
void lcd_reset(void);
void lcd_set_background(int, int, int);
void lcd_writereg(uint8_t, uint16_t);
void lcd_write_pixel(uint16_t, uint16_t, uint16_t);
uint16_t lcd_readreg(uint8_t);
void lcd_rgb_test(void);

/* create a 16 bit RGB pixel */
#define pixel_rgb(r,g,b) ((uint16_t) (((r) & 0xf8) << 8) |\
                                (((g) & 0xfc) << 3) | \
                                (((b) & 0xf8) >> 3))

/* be sure to have included the gpio.h file first */
#define LCD_RESET_PIN   GPIO3
#define LCD_PWM_PIN     GPIO13
#define LCD_CTRL_PORT   GPIOD

/* LCD Registers */
#define DEVICE_CODE_READ  0x00
#define OSC_START         0x00
#define OUTPUT_CTRL       0x01
#define LCD_DRIVE_AC_CTRL 0x02
#define PWR_CTRL_1        0x03
#define DISPLAY_CTRL      0x07
#define FRAME_CYCLE_CTRL  0x0B
#define PWR_CTRL_2        0x0C
#define PWR_CTRL_3        0x0D
#define PWR_CTRL_4        0x0E
#define GATE_SCAN_START   0x0F
#define SLEEP_MODE_1      0x10
#define ENTRY_MODE        0x11
#define SLEEP_MODE_2      0x12
#define GEN_IF_CTRL       0x15
#define PWR_CTRL_5        0x1E
#define RAM_DATA          0x22
#define FRAME_FREQ        0x25
#define ANALOG_SET        0x26
#define VCOM_OTP_1        0x28
#define VCOM_OTP_2        0x29
#define GAMMA_CTRL_1      0x30
#define GAMMA_CTRL_2      0x31
#define GAMMA_CTRL_3      0x32
#define GAMMA_CTRL_4      0x33
#define GAMMA_CTRL_5      0x34
#define GAMMA_CTRL_6      0x35
#define GAMMA_CTRL_7      0x36
#define GAMMA_CTRL_8      0x37
#define GAMMA_CTRL_9      0x3A
#define GAMMA_CTRL_10     0x3B
#define V_RAM_POS         0x44
#define H_RAM_START       0x45
#define H_RAM_END         0x46
#define X_RAM_ADDR        0x4E
#define Y_RAM_ADDR        0x4F

/* Display Size */
#define LCD_DISPLAY_WIDTH   320
#define LCD_DISPLAY_HEIGHT  240
#endif
