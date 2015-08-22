#include "config.h"

#include <util/setbaud.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <avr/io.h>
#include <stdlib.h>

void uart_init(void) {
	UBRR1H = UBRRH_VALUE;
	UBRR1L = UBRRL_VALUE;
	UCSR1B = (1<<RXEN1)|(1<<TXEN1);
	UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);
}

void uart_disable(void) {
	UCSR1B &= ~((1<<RXEN1)|(1<<TXEN1));
}

void uart_enable(void) {
	UCSR1B |= (1<<RXEN1)|(1<<TXEN1);
}

void uart_putc(char c) {
	while (!(UCSR1A & (1<<UDRE1)));
	UDR1 = c;
}

void uart_puts(char *string) {
	uint8_t i = 0;
	while (string[i] != 0x00)
		uart_putc(string[i++]);
}

void uart_puti(int num, uint8_t base) {
	char str[11];
	itoa(num, str, base);
	uart_puts(str);
}

bool uart_getc(char *c) {
	if (UCSR1A&(1<<RXC1)) {
		*c = UDR1;
		return true;
	} else
		return false;
}

void uart_flush() {
	while (UCSR1A&(1<<RXC1)) UDR1;
}
