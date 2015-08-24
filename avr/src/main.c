// System
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>

// Project
#include "config.h"
#include "uart.h"
#include "util.h"
#include "main.h"

// true = up, false = down
SliderPos slider_state = SLIDER_DOWN;
SliderPos slider_wanted = SLIDER_UP;

// Command answer buffer
#define ANS_BUF_SIZE 30
char ans_buf[ANS_BUF_SIZE] = {0x00};

// Timing
#define BEGIN_TIMESTAMP 0
bool time_correct = false;
struct TimeSpec time;
uint64_t timestamp = BEGIN_TIMESTAMP;
uint64_t last_timesync_timestamp = BEGIN_TIMESTAMP;
uint64_t wakeup_timestamp = BEGIN_TIMESTAMP;

uint64_t slider_down_begin_timestamp = BEGIN_TIMESTAMP;
uint64_t slider_up_begin_timestamp = BEGIN_TIMESTAMP;

// Action flags
bool action_updtime = false;
bool wakeup_slider = false;
bool wakeup_button = false;
bool force_sleep = false;
bool action_espreset = false;

// Reed counts
uint16_t button1_count = 0;
uint16_t button2_count = 0;
uint16_t reed_count = 0;

/**
 * get_answer()
 * Writes answer in global ans_buf variable
 * Ignores answers that are longer than ANS_BUF_SIZE
 * Returns false if no answer was received, otherwise true
 */
bool get_answer(uint16_t ms_to_answer) {
	uint8_t ans_buf_iter = 0;
	uint16_t time_ms = 0;

	while (time_ms <= ms_to_answer) {
		char c;
		if (uart_getc(&c)) {
			if (c != '\r' && c != '\n') {
				if (ans_buf_iter + 1 < ANS_BUF_SIZE)
					ans_buf[ans_buf_iter++] = c;
			} else {
				ans_buf[ans_buf_iter] = 0x00;
				return true;
			}
		} else {
			_delay_ms(1);
			time_ms++;
		}
	}

	return false;
}

/**
 * init()
 * Setup ports for input and output
 */
void init() {
	// ESP8266 pins
	sbi(ESP_DDR, ESP_ENABLE);
	sbi(ESP_DDR, ESP_PROGRAM);

	// Reed contact and buttons are connected to GND, so use internal pullups
	sbi(BUTTON_PORT, BUTTON1_BIT);
	sbi(BUTTON_PORT, BUTTON2_BIT);
	sbi(REED_PORT, REED_BIT);

	// Piezo
	sbi(PIEZO_DDR, PIEZO_BIT);

	// Motor relays; on = GND, off = 5v
	// RELAY1 = off, RELAY2 = off: No movement
	// RELAY1 = on, RELAY2 = off: Up
	// RELAY1 = off, RELAY2 = on: Down
	// RELAY1 = on, RELAY2 = on: No movement
	sbi(RELAY_DDR, RELAY1_BIT);
	sbi(RELAY_DDR, RELAY2_BIT);
	sbi(RELAY_PORT, RELAY1_BIT);
	sbi(RELAY_PORT, RELAY2_BIT);

	// Wait for RTC oscillator to stabilize
	_delay_ms(200);

	// RTC oscillator (at the very end, so crystal is more stable)
	// See AVR application note 134 for more information
	TIMSK &= ~((1<<TOIE0) | (1<<OCIE0));
	ASSR |= (1<<AS0);
	TCNT0 = 0;
	TCCR0 = (1<<CS00) | (1<<CS02);
	while(ASSR & ((1<<TCN0UB) | (1<<OCR0UB) || (1<<TCR0UB)));
	TIMSK |= (1<<TOIE0);

	// PF4 - PF7 high to reduce power consumption caused by pullups on the dev board
	sbi(DDRF, 4);
	sbi(DDRF, 5);
	sbi(DDRF, 6);
	sbi(DDRF, 7);
	sbi(PORTF, 4);
	sbi(PORTF, 5);
	sbi(PORTF, 6);
	sbi(PORTF, 7);
}

void esp_startup() {
	/* Start ESP8266 */
	// everything off
	cbi(ESP_PORT, ESP_ENABLE);
	cbi(ESP_PORT, ESP_PROGRAM);
	_delay_ms(500);
	// normal operation
	sbi(ESP_PORT, ESP_PROGRAM);
	_delay_ms(300);
	// startup
	sbi(ESP_PORT, ESP_ENABLE);
	_delay_ms(700);

	uart_flush();
}

/**
 * The step-down converter does not provide consistent current supply when a
 * lot of current is drawn. Therefore, the ESP8266 will only be enabled when
 * the motor / the relays are not active. Since there is no reliable DCF signal
 * and no good WiFi signal transmission while the motors are active, this makes sense.
 */
void esp_disable() {
	cbi(ESP_PORT, ESP_PROGRAM);
	cbi(ESP_PORT, ESP_ENABLE);
}

// Starts up ESP8266 multiple times until the ESP responds, or fails with error beep code
void esp_start_test() {
	// Check if ESP8266 is ok
	// Multiple attempts to start the ESP8266 may be required, since the
	uint8_t tries;
	bool success = false;
	for (tries = 0; tries < 12; ++tries) {
		uart_puts("hello\r\n");
		if (tries != 0 && tries % 2 == 0) esp_startup();
		if (get_answer(100) && strcmp(ans_buf, "hel_ok") == 0) {
			success = true;
			break;
		}
	}

	// ESP8266 not responding, error code deep - long high - deep
	if (!success) {
		uint8_t i;
		for (i = 0; i < 4; i++) {
			sound(7, 300);
			sound(2, 1000);
			sound(7, 300);
			_delay_ms(400);
		}
	}
}

void slider_up() {
	if (slider_wanted != SLIDER_UP) {
		slider_up_begin_timestamp = timestamp;
		esp_disable();
	}
	slider_wanted = SLIDER_UP;
}

void slider_down() {
	if (slider_wanted != SLIDER_DOWN) {
		slider_down_begin_timestamp = timestamp;
		esp_disable();
	}
	slider_wanted = SLIDER_DOWN;
}

/**
 * startup()
 * To be executed after initial startup or after wakeup
 */
void startup() {
	sound(4, 100);

	/** System check / Error reporting **/
	// Twice, WiFi sometimes doesn't work on first startup
	if (slider_state == slider_wanted) {
		esp_startup();
		action_espreset = true;
	}

	// Time has not been synced for a week - warning (long deep - triple high)
	if (timestamp - last_timesync_timestamp > TIME_UPDATE_WARN_AGE || !time_correct) {
		uint8_t i;
		for (i = 0; i < 4; i++) {
			sound(3, 50);
			sound(2, 50);
			sound(1, 50);
			sound(15, 600);
		}
	}

	// Low battery warning (triple high, slow)
	if (battery_voltage_get() <= 120) {
		uint8_t i;
		for (i = 0; i < 4; i++) {
			sound(1, 300);
			_delay_ms(100);
			sound(1, 300);
			_delay_ms(100);
			sound(1, 300);
			_delay_ms(300);
		}
	}

	// Play startup sound
	uart_puts("Huhnix started!\r\n");
	sound(3, 100);
	_delay_ms(30);
	sound(3, 100);
}


/**
 * handle_command()
 * Parse command and execute it, called by handle_shell
 */
void handle_command(char *command, char *args) {
	if (!strcmp(command, "opentime_set")) {
		uint8_t hours = integer_from_string(args, 0);
		uint8_t minutes = integer_from_string(args, 1);
		if (hours <= 25 && minutes < 60) {
			opentime_set(hours, minutes);
			uart_puts("ots_ok\r\n");
		}
	} else if (!strcmp(command, "opentime_get")) {
		uint8_t hours;
		uint8_t minutes;
		opentime_get(&hours, &minutes);
		uart_puti(hours, 10);
		uart_putc(' ');
		uart_puti(minutes, 10);
		uart_puts("\r\n");
	} else if (!strcmp(command, "slider_up")) {
		slider_up();
		uart_puts("slu_ok\r\n");
	} else if (!strcmp(command, "slider_down")) {
		slider_down();
		uart_puts("sld_ok\r\n");
	} else if (!strcmp(command, "systime_get")) {
		uart_puti(time.seconds, 10);
		uart_putc(' ');
		uart_puti(time.minutes, 10);
		uart_putc(' ');
		uart_puti(time.hours, 10);
		uart_putc(' ');
		uart_puti(time.date, 10);
		uart_putc(' ');
		uart_puti(time.month, 10);
		uart_putc(' ');
		uart_puti(time.year, 10);
		uart_putc(' ');
		uart_puti(time_correct ? time.dow : 0, 10);
		uart_puts("\r\n");
	} else if (!strcmp(command, "battery_get")) {
		// Simple linear interpolation for calculating the percentage value
		uint8_t voltage = battery_voltage_get();
		uint8_t percent = 100 * (voltage - BATT_MIN) / (BATT_MAX - BATT_MIN);
		uart_puti(voltage, 10);
		uart_putc(' ');
		uart_puti(percent, 10);
		uart_puts("\r\n");
	}
}

/**
 * handle_shell()
 * Check current UART input and calls command handler
 * Commands may also have arguments, such as
 * setxyz 12
 * Therefore, handle_shell splits the command in two by setting the first space
 * in the command string to 0x00, so that two null-terminated strings are created.
 * If no command arguments are supplied, handle_command will be called with a NULL
 * pointer as args.
 */
void handle_shell() {
	static char cmd_buf[CMD_BUF_SIZE];
	static uint8_t cmd_buf_iter = 0;

	char c;
	if (uart_getc(&c)) {
		// Append to command buffer or execute command
		if (c == '\r' || c == '\n') {
			cmd_buf[cmd_buf_iter] = 0x00;

			// Now dissect the line into actual command and arguments
			uint8_t i;
			uint8_t arg_offset = 0;
			for (i = 0; i < cmd_buf_iter; ++i) {
				if (cmd_buf[i] == ' ') {
					cmd_buf[i] = 0x00;
					arg_offset = i + 1;
					break;
				}
			}

			handle_command(cmd_buf, arg_offset ? &cmd_buf[arg_offset] : NULL);

			cmd_buf_iter = 0;
		} else
			cmd_buf[cmd_buf_iter++] = c;
	}
}

/**
 * handle_slider()
 * Check the current position of the slider door and open / close it if neccesary.
 */
void handle_slider() {
	// Stop conditions
	if (!gbi(REED_PIN, REED_BIT))
		reed_count++;
	else
		reed_count = 0;

	if (slider_wanted == SLIDER_UP && reed_count > INPUT_VALID_COUNT)
		slider_state = SLIDER_UP;

	if (slider_state == SLIDER_UP && slider_wanted == SLIDER_DOWN
			&& timestamp - slider_down_begin_timestamp > SLIDER_DOWN_TIME) {
		slider_state = SLIDER_DOWN;
	}

	if (slider_state == SLIDER_DOWN && slider_wanted == SLIDER_UP
			&& timestamp - slider_up_begin_timestamp > SLIDER_UP_TIME_MAX) {
		slider_state = SLIDER_UP;
	}

	// Relays control conditions
	if (slider_state == SLIDER_UP && slider_wanted == SLIDER_DOWN) {
		cbi(RELAY_PORT, RELAY2_BIT);
		sbi(RELAY_PORT, RELAY1_BIT);
	} else if (slider_state == SLIDER_DOWN && slider_wanted == SLIDER_UP) {
		sbi(RELAY_PORT, RELAY2_BIT);
		cbi(RELAY_PORT, RELAY1_BIT);
	} else if (slider_state == slider_wanted) {
		if (!gbi(RELAY_PORT, RELAY1_BIT) || !gbi(RELAY_PORT, RELAY2_BIT)) action_espreset = true;
		sbi(RELAY_PORT, RELAY2_BIT);
		sbi(RELAY_PORT, RELAY1_BIT);
	}
}

/**
 * handle_time()
 * Updates system time if action_updtime flag is set. Gets DCF77 time from ESP8266.
 * Do not update time while slider is moving, due to motor interference etc. The
 * ESP8266 has to be disabled in that case. Also, waiting for an answer for the
 * ESP will cause precise timing to fail.
 */
void handle_time() {
	if (slider_wanted == slider_state && action_updtime) {
		action_updtime = false;

		uint8_t tries = 0;
		for (tries = 0; tries < 4; ++tries) {
			uart_puts("time_get\r\n");
			if (get_answer(200)) {
				if (strcmp(ans_buf, "x") == 0) return;
				else {
					// Make sure answer is valid by checking if DOW is in range
					uint8_t seconds = integer_from_string(ans_buf, 0);
					uint8_t minutes = integer_from_string(ans_buf, 1);
					uint8_t hours = integer_from_string(ans_buf, 2);
					uint8_t date = integer_from_string(ans_buf, 3);
					uint8_t month = integer_from_string(ans_buf, 4);
					uint8_t year = integer_from_string(ans_buf, 5);
					uint8_t dow = integer_from_string(ans_buf, 6);

					// Some sanity checks
					if (seconds > 59) continue;
					if (minutes > 59) continue;
					if (hours > 23) continue;
					if (date > 31) continue;
					if (month > 12) continue;
					if (dow < 1 || dow > 7) continue;

					time.seconds = seconds;
					time.minutes = minutes;
					time.hours = hours;
					time.date = date;
					time.month = month;
					time.year = year;
					time.dow = dow;
					last_timesync_timestamp = timestamp;
					time_correct = true;
					return;
				}
			}
		}
		action_espreset = true;
	}
}

/**
 * handle_buttons()
 * Slider buttons:	BUTTON1 --> Slider up
 *					BUTTON2 --> Slider down
 */
void handle_buttons() {
	// Input validation
	if (!gbi(BUTTON_PIN, BUTTON1_BIT))
		button1_count++;
	else
		button1_count = 0;

	if (!gbi(BUTTON_PIN, BUTTON2_BIT))
		button2_count++;
	else
		button2_count = 0;

	// Slider movement
	if (button1_count > INPUT_VALID_COUNT)
		slider_up();

	if (button2_count > INPUT_VALID_COUNT)
		slider_down();

	// Sleep
	if (button1_count > INPUT_VALID_COUNT && button2_count > INPUT_VALID_COUNT) {
		slider_wanted = slider_state;
		force_sleep = true;
	}
}

/**
 * handle_sleep()
 * Makes the AVR go to sleep if time information has been received and no
 * action is pending. Wakes up if either a button was pressed or open
 * action is required.
 */
void handle_sleep() {
	// Go to sleep when:
	// 1. No current slider action
	// 2. Time has been synchronized or microcontroller has been awake for too long
	if (((force_sleep || timestamp - wakeup_timestamp > MAX_WAKE_TIME) ||
			(time_correct && timestamp - last_timesync_timestamp < TIME_UPDATE_MAXAGE)) &&
			slider_wanted == slider_state) {
		sound(2, 200);
		sound(3, 200);
		sound(4, 200);
		sound(5, 200);
		sound(6, 200);

		// Disable UART
		uart_disable();

		// Disable ESP8266
		cbi(ESP_PORT, ESP_ENABLE);
		cbi(ESP_PORT, ESP_PROGRAM);

		wakeup_button = false;
		wakeup_slider = false;

		// Enable button interrupts
		BUTTON_EICR |= BUTTON1_ISC | BUTTON2_ISC;
		EIMSK |= BUTTON1_INT | BUTTON2_INT;

		// Sleep loop
		while(!wakeup_button && !wakeup_slider) {
			set_sleep_mode(SLEEP_MODE_PWR_SAVE);
			sleep_enable();
			sleep_cpu();

			// Sleeping Here

			sleep_disable();
		}

		wakeup_timestamp = timestamp;
		force_sleep = false;

		// Disable button interrupts
		EIMSK ^= BUTTON1_INT | BUTTON2_INT;

		// Enable UART
		uart_enable();

		// Startup sound and enable ESP8266
		startup();

		// When both buttons were pressed for wakeup, make sure there
		// is time for them to be released before triggering slider movement
		_delay_ms(400);
	}
}

/**
 * Reset ESP8266 board whenever neccesary - e.g. after a slider movement
 * or after a wakeup from power save mode.
 */
void handle_esp() {
	if (action_espreset && !force_sleep) {
		action_espreset = false;
		esp_start_test();
	}
}

int main(void) {
	sei();
	uart_init();
	init();
	startup();

	while (1) {
		// Handle inputs
		handle_time();
		handle_shell();
		handle_buttons();

		// Hanlde output
		handle_slider();
		handle_esp();

		// Handle sleep
		handle_sleep();

		_delay_us(10);
	}

	while(1);
}

/*** INTERRUPTS ***/
ISR(TIMER0_OVF_vect) {
	// Increase timestamp
	timestamp++;
	if (timestamp % TIME_UPDATE_INTERVAL == 0)
		action_updtime = true;

	// Increase time
	if (++(time.seconds) >= 60) {
		time.seconds = 0;
		if (++(time.minutes) >= 60) {
			time.minutes = 0;
			if (++(time.hours) >= 24) {
				time.hours = 0;
				++time.date;
				// ignore any further than this, the DCF77 time signal will
				// handle these cases. A correct date is also not neccessary
				// for the function of the device.
			}
		}
	}

	// Check if slider needs to be opened (every 60 seconds)
	if (time_correct && time.seconds == 0) {
		uint8_t hours;
		uint8_t minutes;
		opentime_get(&hours, &minutes);
		if (hours == 25) return; // just to make sure
		if (time.hours == hours && time.minutes == minutes) {
			wakeup_slider = true;
			slider_up();
		}
	}
}

ISR(BUTTON1_vect) {
	if (!gbi(BUTTON_PIN, BUTTON2_BIT)) wakeup_button = true;
}

ISR(BUTTON2_vect) {
	if (!gbi(BUTTON_PIN, BUTTON1_BIT)) wakeup_button = true;
}

ISR(__vector_default) {}
