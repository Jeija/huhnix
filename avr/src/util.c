#include <util/delay.h>
#include <avr/eeprom.h>
#include <stdlib.h>

#include "config.h"
#include "util.h"

void opentime_set(uint8_t hours, uint8_t minutes)
{
	eeprom_write_byte((uint8_t *)EEPROM_OFFSET, hours);
	eeprom_write_byte((uint8_t *)EEPROM_OFFSET + 1, minutes);
}

void opentime_get(uint8_t *hours, uint8_t *minutes)
{
	*hours = eeprom_read_byte((uint8_t *)EEPROM_OFFSET);
	*minutes = eeprom_read_byte((uint8_t *)EEPROM_OFFSET + 1);
}

/**
 * Returns the nth integer from a string in which integers are separated by spaces
 * The integer must be an unsigned 8-bit integer, the string must be null-terminated
 * Returns 255 on error (nth too high), but string must contain valid integers
 */
uint8_t integer_from_string(char *buf, uint8_t nth) {
	char numberbuf[4];
	uint8_t numberbuf_iter = 0;
	uint8_t i = 0;
	uint8_t intnum = 0;
	while (buf[i] != 0x00) {
		if (buf[i] == ' ') {
			intnum++;
			if (intnum > nth) break;
		} else if (intnum == nth) {
			numberbuf[numberbuf_iter++] = buf[i];
		}

		i++;
	}

	if (intnum < nth) return 255;

	numberbuf[numberbuf_iter] = 0x00;
	return (uint8_t) atoi(numberbuf);
}

void sound(uint8_t tone, uint16_t duration) {
	uint32_t i;
	for (i = 0; i < duration / tone * (1000 / 100); ++i) {
		tbi(PIEZO_PORT, PIEZO_BIT);

		uint8_t t = 0;
		for (t = 0; t < tone; ++t) _delay_us(100);
	}
	cbi(PIEZO_PORT, PIEZO_BIT);
}

uint8_t battery_voltage_get(void) {
	// ADC measurement
	ADCSRA = (1<<ADEN) | ((1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0));
	ADMUX = (1<<REFS1) | (1<<REFS0) | BATT_ADC;
	ADCSRA |= (1<<ADSC);
	while (ADCSRA & (1<<ADSC));
	uint16_t adc_result = ADCW;

	// Convert ADC value to 100mV value
	return (uint8_t)(adc_result / 1024. * 2.56 * BATT_ADC_FACTOR);
}
