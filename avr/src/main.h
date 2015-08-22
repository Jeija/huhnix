#ifndef _MAIN_H
#define _MAIN_H

typedef enum _SliderPos {
	SLIDER_UP = 0,
	SLIDER_DOWN
} SliderPos;

struct TimeSpec {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t date;
	uint8_t month;
	uint8_t year;
	uint8_t dow;
};

#endif
