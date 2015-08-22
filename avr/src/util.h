#ifndef _UTIL_H
#define _UTIL_H

// Bit helper functions
#define sbi(ADDR, BIT) ((ADDR |=  (1<<BIT)))
#define cbi(ADDR, BIT) ((ADDR &= ~(1<<BIT)))
#define gbi(ADDR, BIT) ((ADDR &   (1<<BIT)))
#define tbi(ADDR, BIT) ((ADDR ^=  (1<<BIT)))

// EEPROM opentime memory
void opentime_set(uint8_t hours, uint8_t minutes);
void opentime_get(uint8_t *hours, uint8_t *minutes);

// String to number conversion
uint8_t integer_from_string(char *buf, uint8_t nth);

// Piezo, duration in ms, no accurate frequency selection
// (not really neccesary for this application)
void sound(uint8_t tone, uint16_t duration);

// Battery status, voltage in 100mV
uint8_t battery_voltage_get(void);

#endif
