/*
 * Simple include file for our utility routines
 */

void uart_setup(int baud);
char uart_getc(int);
void uart_putc(char);
void uart_puts(char *);
void led_setup(void);
void msleep(uint32_t);
void systick_setup(void);
uint32_t mtime(void);
void clock_setup(void);

#ifndef CONSOLE_UART
#define CONSOLE_UART    USART2
#endif
