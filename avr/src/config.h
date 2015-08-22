// UART1 configuration
#define BAUD 9600

// Buffer configurations, internals, ...
#define CMD_BUF_SIZE 40

// EEPROM layout
#define EEPROM_OFFSET 0x00

/*** Hardware connections ***/
// Buttons
#define BUTTON_PORT PORTE
#define BUTTON_DDR DDRE
#define BUTTON_PIN PINE
#define BUTTON1_BIT PE4
#define BUTTON2_BIT PE6
#define BUTTON_EICR EICRB
#define BUTTON1_ISC 0x00
#define BUTTON2_ISC 0x00
#define BUTTON1_INT (1<<INT4)
#define BUTTON2_INT (1<<INT6)
#define BUTTON1_vect INT4_vect
#define BUTTON2_vect INT6_vect

// Slider reed contact
#define REED_PORT PORTF
#define REED_DDR DDRF
#define REED_PIN PINF
#define REED_BIT PF1

// Slider motors (to relays)
#define RELAY_PORT PORTB
#define RELAY_DDR DDRB
#define RELAY1_BIT PB2
#define RELAY2_BIT PB4

// Piezo loudspeaker
#define PIEZO_PORT PORTE
#define PIEZO_DDR DDRE
#define PIEZO_BIT PE7

// Battery, factor depends on voltage devider specifics
#define BATT_ADC 2
#define BATT_MIN 115
#define BATT_MAX 130
#define BATT_ADC_FACTOR 116

// ESP8266
#define ESP_PORT PORTA
#define ESP_DDR DDRA
#define ESP_ENABLE PA0
#define ESP_PROGRAM PA1

// Input value counts
#define INPUT_VALID_COUNT 1000
#define SLIDER_UP_EMERGENCY_STOP_COUNT 1200000

// Timing
#define SLIDER_DOWN_TIME 6
#define SLIDER_UP_TIME_MAX 25
#define TIME_UPDATE_INTERVAL 10
#define TIME_UPDATE_MAXAGE 20
#define TIME_UPDATE_WARN_AGE ((uint16_t)3600 * 24 * 7)
#define MAX_WAKE_TIME (60 * 6)
