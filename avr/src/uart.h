#include <stdbool.h>

#ifndef _UART_H
#define _UART_H

// This uses UART1 (RXD1 / TXD1)!

void uart_init(void);
void uart_putc(char c);
void uart_puts(char *string);
void uart_puti(int num, uint8_t base);

// Get one character, returns false if no character was available
bool uart_getc(char *c);

// Flush receive buffer
void uart_flush();

// Disable / Re-Enable UART. Only call uart_enable after disabling the UART. Saves power.
void uart_disable();
void uart_enable();

#endif
