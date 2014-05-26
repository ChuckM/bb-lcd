/*
 * Copyright (C) 2013 Chuck McManis (cmcmanis@mcmanis.com)
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <stdint.h>
#include <libopencm3/cm3/assert.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include "util.h"

/* monotonically increasing number of milliseconds from reset
 * overflows every 49 days if you're wondering
 */
static volatile uint32_t system_millis;

/* Called when systick fires */
void sys_tick_handler(void) {
    system_millis++;
}

/* sleep for delay milliseconds */
void
msleep(uint32_t delay) {
    uint32_t wake = system_millis + delay;
    while (wake > system_millis) ;
}

/* Return the current notion of the time */
uint32_t
mtime() {
    return system_millis;
}

/* Set up a timer to create 1mS ticks. */
void 
systick_setup(void)
{
    /* clock rate / 1000 to get 1mS interrupt rate */
    systick_set_reload(168000);
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_counter_enable();
    /* this done last */
    systick_interrupt_enable();
}

/* Set STM32 to 168 MHz. */
void clock_setup(void)
{
	rcc_clock_setup_hse_3v3(&hse_8mhz_3v3[CLOCK_3V3_168MHZ]);

	/* Enable GPIOD clock for LED & USARTs. */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPDEN);
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPCEN);

	/* Enable clocks for USART6. */
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_USART6EN);
}

#define RECV_BUFSIZE 32
static char recv_buf[RECV_BUFSIZE];
static volatile int buf_ndx;
static volatile int read_ndx;

/* 
 * If we hit this ISR it is because we got a received character
 * interrupt from the UART.
 */
void usart6_isr() {
    recv_buf[buf_ndx++] = USART_DR(USART2);
    buf_ndx %= RECV_BUFSIZE;
}

/* Configure USART2 as a 38400, 8N1 serial port that has an interrupt
 * driven receive function (transmit is blocking)
 */
void uart_setup(int baud)
{
	/* Setup GPIO pins for USART2 transmit. */
	gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6);
	gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO7);

	gpio_set_output_options(GPIOC, GPIO_OTYPE_OD, GPIO_OSPEED_25MHZ, GPIO7);

	gpio_set_af(GPIOC, GPIO_AF8, GPIO6);
	gpio_set_af(GPIOC, GPIO_AF8, GPIO7);

	/* Setup USART6 parameters. */
	usart_set_baudrate(USART6, baud);
	usart_set_databits(USART6, 8);
	usart_set_stopbits(USART6, USART_STOPBITS_1);
	usart_set_mode(USART6, USART_MODE_TX_RX);
	usart_set_parity(USART6, USART_PARITY_NONE);
	usart_set_flow_control(USART6, USART_FLOWCONTROL_NONE);

    /* Allow for receive interrupts */
    buf_ndx = 0;
    read_ndx = 0;
    nvic_enable_irq(NVIC_USART6_IRQ);

	/* Finally enable the USART. */
	usart_enable(USART6);
    usart_enable_rx_interrupt(USART6);
	/* Setup USART2 parameters. */
	usart_set_baudrate(USART2, 38400);
	usart_set_databits(USART2, 8);
	usart_set_stopbits(USART2, USART_STOPBITS_1);
	usart_set_mode(USART2, USART_MODE_TX_RX);
	usart_set_parity(USART2, USART_PARITY_NONE);
	usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

    /* Allow for receive interrupts */
    buf_ndx = 0;
    read_ndx = 0;
    nvic_enable_irq(NVIC_USART2_IRQ);

	/* Finally enable the USART. */
	usart_enable(USART2);
    usart_enable_rx_interrupt(USART2);
}

/*
 * uart_getc()
 *
 * Read a character from the recieve buffer if it is there. You can
 * call this with a '1' to wait for a character to appear (blocking)
 * or with a '0' which will return a character if one is available
 * otherwise it returns NUL (aka 0). 
 */
char
uart_getc(int wait) {
    char res;

    if ((read_ndx == buf_ndx) && ! wait) {
        return '\000';
    }
    /* wait for a character */
    while (read_ndx == buf_ndx);
    res = recv_buf[read_ndx++];
    /* roll over */
    read_ndx %= RECV_BUFSIZE;
    return res;
}

/*
 * uart_putc(char c)
 *
 * Write a character the uart (Blocking). This does what it says,
 * puts out a character to the serial port. If one is in the process
 * of being sent it waits until it finishes then puts this one out.
 */
void
uart_putc(char c) {
    while (!(USART_SR(USART6) & USART_SR_TXE)) {
            __asm__("NOP");
    }
    USART_DR(USART6) = (uint16_t)(c & 0xff);
}

/*
 * uart_puts(char *string)
 *
 * Write a NUL terminated string to the UART. This routine writes
 * the characters in order (adding LF to CR). 
 */
void
uart_puts(char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}


char *stime(uint32_t);

/*
 * stime(uint32_t)
 *
 * Convert a number representing milliseconds into a 'time' string
 * of HHH:MM:SS.mmm where HHH is hours, MM is minutes, SS is seconds
 * and .mmm is fractions of a second.
 */
char *
stime(uint32_t t) {
    static char time_string[14];
    uint16_t msecs = t % 1000;
    uint8_t secs = (t / 1000) % 60;
    uint8_t mins = (t / 60000) % 60;
    uint16_t hrs = (t /3600000);

    // HH:MM:SS.mmm\0
    // 0123456789abc
    time_string[0] = (hrs % 100) + '0';
    time_string[1] = (hrs / 10) % 10 + '0';
    time_string[2] = hrs % 10 + '0';
    time_string[3] = ':';
    time_string[4] = (mins / 10)  % 10 + '0';
    time_string[5] = mins % 10 + '0';
    time_string[6] = ':';
    time_string[7] = (secs / 10)  % 10 + '0';
    time_string[8] = secs % 10 + '0';
    time_string[9] = '.';
    time_string[10] = (msecs / 100) % 10 + '0';
    time_string[11] = (msecs / 10) % 10 + '0';
    time_string[12] = msecs % 10 + '0';
    time_string[13] = 0;
    return &time_string[0];
}


