// System
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"

// HTTPD
#include "webpages-espfs.h"
#include "httpdespfs.h"
#include "httpd.h"

// Configuration
#include "user_config.h"

bool time_valid = false;
struct TimeSpec {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t date;
	uint8_t month;
	uint8_t year;
	uint8_t dow;
} time;

bool dcf_decoder_readjust = false;

os_timer_t dcf_read_timer;
os_timer_t dcf_decode_timer;
os_timer_t time_inc_timer;

#define ANS_BUF_SIZE 30
uint8_t ans_buf_iter = 0;
char ans_buf[ANS_BUF_SIZE];

#define CMD_BUF_SIZE 16
uint8_t cmd_buf_iter = 0;
char cmd_buf[CMD_BUF_SIZE];

uint8_t dcf_hc = 0;
uint8_t dcf_lc = 0;

uint8_t signal[60];
uint8_t signal_iter = 0;

#define user_procTaskPrio 0
#define user_procTaskQueueLen 1
os_event_t user_procTaskQueue[user_procTaskQueueLen];

// Returns FALSE if value is invalid
uint8_t decode_value(uint8_t firstbit, uint8_t lastbit) {
	++firstbit; ++lastbit;
	uint8_t bitnum = lastbit - firstbit;
	uint8_t value = 0;

	// No need for math.h's pow() this way
	if (signal[firstbit]) value += 1;
	if (bitnum >= 1 && signal[firstbit + 1]) value += 2;
	if (bitnum >= 2 && signal[firstbit + 2]) value += 4;
	if (bitnum >= 3 && signal[firstbit + 3]) value += 8;

	// BCD notation
	if (bitnum >= 4 && signal[firstbit + 4]) value += 10;
	if (bitnum >= 5 && signal[firstbit + 5]) value += 20;
	if (bitnum >= 6 && signal[firstbit + 6]) value += 40;
	if (bitnum >= 7 && signal[firstbit + 7]) value += 80;

	return value;
}

bool check_parity(uint8_t firstbit, uint8_t lastbit, uint8_t paritybit) {
	++firstbit; ++lastbit; ++paritybit;
	uint8_t i;
	uint8_t parity = 0;
	for (i = firstbit; i <= lastbit; i++)
		parity ^= signal[i];

	return (parity == signal[paritybit]);
}

void dcf2time(void) {
	if (signal_iter == 60) {
		time.seconds = 0;

		// Assume time signal is good only if all parities are correct
		if (check_parity(21, 27, 28) && check_parity(29, 34, 35) && check_parity(36, 57, 58)) {
			time.minutes = decode_value(21, 27);
			time.hours = decode_value(29, 34);
			time.date = decode_value(36, 41);
			time.dow = decode_value(42, 44);
			time.month = decode_value(45, 49);
			time.year = decode_value(50, 57);

			time_valid = true;
		}
	}
}

/*** Timers ***/
void dcf_decode_timer_cb(void) {
	// Last second of minute, no time signal, readjust
	if (dcf_hc <= 10) {
		dcf2time();
		signal_iter = 0;
		os_timer_disarm(&dcf_decode_timer);
		dcf_decoder_readjust = true;
	}

	if (signal_iter < 60) signal[signal_iter++] = (dcf_hc > 35);
	dcf_lc = 0;
	dcf_hc = 0;
}

void dcf_decode_timer_adjust(void) {
	os_timer_disarm(&dcf_decode_timer);
	os_timer_setfn(&dcf_decode_timer, (os_timer_func_t *) dcf_decode_timer_cb, NULL);
	os_timer_arm(&dcf_decode_timer, 1000, 1);
}

void dcf_read_timer_cb(void)
{
	// Inverted signal, add to low count (lc) when pin is true
	if (GPIO_INPUT_GET(2)) {
		dcf_lc++;
		if (dcf_decoder_readjust) {
			dcf_decode_timer_adjust();
			dcf_decoder_readjust = false;
		}
	} else
		dcf_hc++;
}

void time_inc_timer_cb(void) {
	time.seconds++;
	if (time.seconds == 60) {
		time.seconds = 0;
		time.minutes++;
		if (time.minutes == 60) {
			time.minutes = 0;
			time.hours++;
			if (time.hours == 24) {
				time.hours = 0;
				time.date++;
				time.dow++;
				// Ignore the rest, receive that from DCF77
			}
		}
	}
}

void ap_init(void) {
	// Check if configuration is ok already (compare SSID)
	struct softap_config netcfg;

	wifi_softap_get_config(&netcfg);
	if (os_strcmp(WIFI_SSID, netcfg.ssid) == 0) return;

	// Configuration is not good, change config and reboot
	system_restore();
	os_memcpy(netcfg.ssid, WIFI_SSID, os_strlen(WIFI_SSID));
	netcfg.ssid[os_strlen(WIFI_SSID)] = 0x00;
	netcfg.ssid_len = os_strlen(WIFI_SSID);
	netcfg.channel = 1;
	netcfg.authmode = AUTH_OPEN;
	netcfg.beacon_interval = 100;

	wifi_softap_set_config(&netcfg);
	wifi_set_opmode(0x02);
	system_restart();
}

/*** Loop ***/
static void ICACHE_FLASH_ATTR loop(os_event_t *events) {
	char c;
	int res = uart_rx_one_char(&c);

	if(res != 1) {
		if (c != '\r' && c != '\n') {
			if (cmd_buf_iter + 1 < CMD_BUF_SIZE)
				cmd_buf[cmd_buf_iter++] = c;
		} else {
			cmd_buf[cmd_buf_iter] = 0x00;
			cmd_buf_iter = 0;
			if (os_strcmp(cmd_buf, "time") == 0) {
				if (time_valid) {
					os_printf("Zeit: %d.%d.%d, %d:%d:%d, DOW %d\r\n", time.date, time.month, time.year, time.hours, time.minutes, time.seconds, time.dow);
				} else {
					os_printf("No valid time information yet!\r\n");
				}
			} else if (os_strcmp(cmd_buf, "dcf") == 0) {
				os_printf("DCF77 status information:\r\n");
				os_printf("Received bits this minute: %d\r\n", signal_iter);
				os_printf("High count %d, Low count %d, Total %d\r\n", dcf_hc, dcf_lc, dcf_hc + dcf_lc);
				os_printf("Signal buffer:\r\n");
				uint8_t i;
				for (i = 0; i < 60; i++) os_printf("%d", signal[i]);
				os_printf("\r\n");
			} else if (os_strcmp(cmd_buf, "time_get") == 0) {
				if (time_valid) {
					os_printf("%d %d %d %d %d %d %d\r\n", time.seconds, time.minutes, time.hours, time.date, time.month, time.year, time.dow);
				} else {
					os_printf("x\r\n");
				}
			} else if (os_strcmp(cmd_buf, "hello") == 0) {
				os_printf("hel_ok\r\n");
			}
		}
	}
	system_os_post(user_procTaskPrio, 0, 0);
}

/**
 * get_answer
 * Writes answer in global ans_buf variable
 * Ignores answers that are longer than ANS_BUF_SIZE
 * Returns false if no answer was received, otherwise true
 *
 * ms_to_answer maximum 1000ms due to watchdog!
 */
bool get_answer(uint16_t ms_to_answer) {
	ans_buf_iter = 0;
	uint16_t time_ms = 0;

	while (time_ms <= ms_to_answer) {
		char c;
		int res = uart_rx_one_char(&c);

		if (res == 1) {
			os_delay_us(1000);
			time_ms++;
		} else {
			if (c != '\r' && c != '\n') {
				if (ans_buf_iter + 1 < ANS_BUF_SIZE)
					ans_buf[ans_buf_iter++] = c;
			} else {
				ans_buf[ans_buf_iter] = 0x00;
				return true;
			}
		}
	}

	return false;
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

/**
 * HTTP commands
 * They forward the corresponding command to the AVR controller and decode the
 * answer. The may then forward that answer to the HTTP client in a human-readable
 * format.
 */
int cmd_slider_up(HttpdConnData *conn) {
	uint8_t i;
	for (i = 0; i < 4; i++) {
		os_printf("\r\nslider_up\r\n");
		bool res = get_answer(200);
		if (res == true && os_strcmp("slu_ok", ans_buf) == 0) {
			os_printf("Leaving with success state\r\n");
			httpdSend(conn, "ok", -1);
			return HTTPD_CGI_DONE;
		}
	}

	os_printf("Leaving with error state, buffer is %s\r\n", ans_buf);
	httpdSend(conn, "Keine Antwort vom AVR-Controller", -1);
	return HTTPD_CGI_DONE;
}

int cmd_slider_down(HttpdConnData *conn) {
	uint8_t i;
	for (i = 0; i < 4; i++) {
		os_printf("\r\nslider_down\r\n");
		bool res = get_answer(200);
		if (res == true && os_strcmp("sld_ok", ans_buf) == 0) {
			httpdSend(conn, "ok", -1);
			return HTTPD_CGI_DONE;
		}
	}

	httpdSend(conn, "Keine Antwort vom AVR-Controller", -1);
	return HTTPD_CGI_DONE;
}

int cmd_opentime_get(HttpdConnData *conn) {
	uint8_t i;
	for (i = 0; i < 4; i++) {
		os_printf("\r\nopentime_get\r\n");
		bool res = get_answer(200);
		if (res == true) {
			uint8_t hours = integer_from_string(ans_buf, 0);
			uint8_t minutes = integer_from_string(ans_buf, 1);

			char resbuf[50];
			if (hours == 25)
				os_sprintf(resbuf, "Klappe wird nicht automatisch geöffnet");
			else
				os_sprintf(resbuf, "Aufmachzeit ist %d:%d Uhr", hours, minutes);
			httpdSend(conn, resbuf, -1);
			return HTTPD_CGI_DONE;
		}
	}

	httpdSend(conn, "Keine Antwort vom AVR-Controller", -1);
	return HTTPD_CGI_DONE;
}

int cmd_systime_get(HttpdConnData *conn) {
	uint8_t i;
	for (i = 0; i < 4; i++) {
		os_printf("\r\nsystime_get\r\n");
		bool res = get_answer(200);
		if (res == true) {
			uint8_t seconds = integer_from_string(ans_buf, 0);
			uint8_t minutes = integer_from_string(ans_buf, 1);
			uint8_t hours = integer_from_string(ans_buf, 2);
			uint8_t date = integer_from_string(ans_buf, 3);
			uint8_t month = integer_from_string(ans_buf, 4);
			uint8_t year = integer_from_string(ans_buf, 5);
			uint8_t dow = integer_from_string(ans_buf, 6);

			char dow_readable[30];
			if (dow == 255) continue;
			else if (dow == 1) os_strcpy(dow_readable, "Montag");
			else if (dow == 2) os_strcpy(dow_readable, "Dienstag");
			else if (dow == 3) os_strcpy(dow_readable, "Mittwoch");
			else if (dow == 4) os_strcpy(dow_readable, "Donnerstag");
			else if (dow == 5) os_strcpy(dow_readable, "Freitag");
			else if (dow == 6) os_strcpy(dow_readable, "Samstag");
			else if (dow == 7) os_strcpy(dow_readable, "Sonntag");
			else if (dow == 0) os_strcpy(dow_readable, "Ungültige Systemzeit");

			char resbuf[80];
			os_sprintf(resbuf, "%s, %d.%d.%d %d:%d:%d Uhr", dow_readable, date, month, year, hours, minutes, seconds);
			httpdSend(conn, resbuf, -1);
			return HTTPD_CGI_DONE;
		}
	}

	httpdSend(conn, "Keine Antwort vom AVR-Controller", -1);
	return HTTPD_CGI_DONE;
}

int cmd_battery_get(HttpdConnData *conn) {
	uint8_t i;
	for (i = 0; i < 4; i++) {
		os_printf("\r\nbattery_get\r\n");
		bool res = get_answer(200);
		if (res == true) {
			uint8_t voltage = integer_from_string(ans_buf, 0);
			uint8_t percent = integer_from_string(ans_buf, 1);

			char resbuf[50];
			os_sprintf(resbuf, "Batteriespannung ist %u.%uV, geschätzt %u%", (voltage - voltage % 10) / 10, voltage % 10, percent);
			httpdSend(conn, resbuf, -1);
			return HTTPD_CGI_DONE;
		}
	}

	httpdSend(conn, "Keine Antwort vom AVR-Controller", -1);
	return HTTPD_CGI_DONE;
}

int cmd_opentime_set(HttpdConnData *conn) {
	// Parse command from GET parameters
	char hours_str[5];
	char minutes_str[5];

	httpdFindArg(conn->getArgs, "hours", hours_str, sizeof(hours_str));
	httpdFindArg(conn->getArgs, "minutes", minutes_str, sizeof(minutes_str));

	char command[30];
	os_sprintf(command, "\r\nopentime_set %s %s\r\n", hours_str, minutes_str);

	// Send command / wait for answer
	uint8_t i;
	for (i = 0; i < 4; i++) {
		os_printf(command);
		bool res = get_answer(200);
		if (res == true && os_strcmp("ots_ok", ans_buf) == 0) {
			httpdSend(conn, "ok", -1);
			return HTTPD_CGI_DONE;
		}
	}

	httpdSend(conn, "Keine Antwort vom AVR-Controller", -1);
	return HTTPD_CGI_DONE;
}

int cmd_dcf_info(HttpdConnData *conn) {
	char buf[500];
	os_sprintf(buf, "DCF77 status information:\r\nReceived bits this minute: %d\r\nHigh count %d, Low count %d, Total %d\r\nSignal buffer:\r\n", signal_iter, dcf_hc, dcf_lc, dcf_hc + dcf_lc);

	uint16_t begin = 0;
	uint8_t i;
	while (buf[++begin]);
	for (i = 0; i < 60; ++i) buf[begin + i] = signal[i] ? '1' : '0';
	buf[begin + i] = 0x00;

	httpdSend(conn, buf, -1);
	return HTTPD_CGI_DONE;
}

HttpdBuiltInUrl builtInUrls[] = {
	{"/", cgiRedirect, "/index.html"},
	{"/slider_up", cmd_slider_up, NULL},
	{"/slider_down", cmd_slider_down, NULL},
	{"/opentime_get", cmd_opentime_get, NULL},
	{"/systime_get", cmd_systime_get, NULL},
	{"/battery_get", cmd_battery_get, NULL},
	{"/opentime_set", cmd_opentime_set, NULL},
	{"/dcf_info", cmd_dcf_info, NULL},
	{"*", cgiEspFsHook, NULL},
	{NULL, NULL, NULL}
};

/*** Main function ***/
void ICACHE_FLASH_ATTR user_init() {
	uart_div_modify(0, UART_CLK_FREQ / BAUD);
	os_printf("Startup from %d...\r\n", system_get_rst_info()->reason);
	gpio_init();
	ap_init();

	// HTTPD
	espFsInit((void*)(webpages_espfs_start));
	httpdInit(builtInUrls, 80);

	// Set GPIO2 (DCF77 pin) to input, disable pullup
	gpio_output_set(0, 0, 0, 2);
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);

	// DCF77 read timer
	os_timer_disarm(&dcf_read_timer);
	os_timer_setfn(&dcf_read_timer, (os_timer_func_t *) dcf_read_timer_cb, NULL);
	os_timer_arm(&dcf_read_timer, 5, 1);

	// Second increase timer
	os_timer_disarm(&time_inc_timer);
	os_timer_setfn(&time_inc_timer, (os_timer_func_t *) time_inc_timer_cb, NULL);
	os_timer_arm(&time_inc_timer, 1000, 1);

	// DCF77 decode timer: decide wheter 1 or 0
	dcf_decode_timer_adjust();

	os_printf(" completed!\r\n\r\n");
	system_os_task(loop, user_procTaskPrio, user_procTaskQueue, user_procTaskQueueLen);
	system_os_post(user_procTaskPrio, 0, 0);
}
