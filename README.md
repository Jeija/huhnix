# Huhnix
Huhnix is software for an automatic slider door, such as a door for a hen house (hence the name). The slider door opens automatically every morning at a set time and is operated with two buttons or via WiFi. Settings can be adjusted with WiFi device that can access the settings webpage. Huhnix uses two microprocessors that communicate over a UART interface.

## Pictures
<img alt="Slider with electronics" src="http://i.imgur.com/HPdC5RX.jpg" width="400">
<img alt="Configuration site on smartphone" src="http://i.imgur.com/I2MfUiD.jpg" width="400">
<img alt="Henhouse with slider and buttons" src="http://i.imgur.com/Z5bKxQ1.jpg" width="400">
<img alt="...and a chicken" src="http://i.imgur.com/DBwpBMd.jpg" width="400">

## Documentation
### Main controller: AVR
The main processor receives inputs from two buttons that are used to make the doors slide up or down and keeps all the settings and keeps track of the time (so no need for a RTC).

### Secondary controller: ESP8266
The ESP8266 provides a configuration interface by acting as an access point that mobile devices can connect to in order to access a simple web interface. This web interface can be used to access system information such as the system clock and battery levels. The ESP8266 (e.g. within an ESP-01 module) is only active, when the main processor wakes it up. That is the case, when any of the slider control buttons have been pressed. This is crucial to save limited battery power. The battery can be charged using a solar panel. The ESP8266 software is based on the IoT SDK version 1.4 from Espressif.

### Controller communication protocol
#### Definitions
Due to restrictions of the ESP-01 module, the two processors have to communicate over the serial port. The protocol is as follows:
* Baudrate 9600 Baud, no parity, 8 bits, no stopbit, no flow control
* Each processor must always listen for commands and can always send commands. When a processor receives a command, it must answer immediately or perform the requested action and may not execute another command at the same time.
* If any unknown or unexpected string / command / ... is received by any processor, it has to be ignored. This is due to the fact that the ESP8266 SDK ROM outputs debug information, which cannot be disabled.
* Each command is terminated by "\r\n". The command itself may not contain any of these characters.

#### Commands
| ESP8266 command	| Description	|
|-----------------	|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------	|
| time	| For debugging only, outputs human-readable DCF77 time information	|
| dcf	| For debugging only, outputs debugging data on the DCF77 signal	|
| time_get	| To be used by the AVR controller. Outputs time data in a machine (and also human) readable form: Seconds, minutes, hours, date (of month), month, year (only last two digits) and day (of week) are transferred in this order as human-readable strings with a space in between each value. If no valid time value is available, the ESP8266 answers with the character x.	|

| AVR command	| Description	|
|---------------------	|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------	|
| slider_down	| Move the slider door down. Must be acknowledged by sending the string "sld_ok".	|
| slider_up	| Move the slider door up. Must be acknowledged by sending the string "slu_ok".	|
| systime_get	| Gets the current system time in the same format as time_get (for debugging and for Display on the web frontend).	|
| battery_get	| To be used by the ESP8266. Voltage and percent values are transferred as two human-readable numbers with a space in between them. The voltage value is transferred as an unsigned integer with the value being measured in 100mV (so a value of 123 indicates 12,3V). The percentage value is an estimated integer number between 0 and 100.	|
| opentime_get	| Get time at which to open the slider automatically (time of day). Hours and minutes are transferred as two human-readable integer numbers with a space in between them. An hour value of 25 indicates that the slider is not supposed to be opened at all.	|
| opentime_set <hours> <minutes>	| Tell AVR to open the slider at <hours>:<minutes> each day, where hours and minutes are integers seperate by a space. Must be acknowledged by sending the string "ots_ok"	|

### AVR hardware
The ATMega128A's fusebits are `E:FF, H:89, L:FF`, make sure to disable ATMega 103 compatibility mode! I use a cheap [chinese ATMega128A development board from LC Studio](http://www.lctech-inc.com/Hardware/Detail.aspx?id=68611f14-46cd-4676-95df-f1246689dbba) with the power LED desoldered in order to save power. A custom addon board (PCB design files in `hardware` directory, currently only tested with a prototype board) is stacked on top of the pin headers of the development board. This addon board has connectors for the DCF77 real time clock (I use the one from [conrad.de](http://www.conrad.de/ce/de/product/641138), for buttons, for 12V power sensing, the 5V power supply (provided by a step down converter from a lead-acid battery) and for a 2-channel relay board (HL-52 V 1.0).

## Attribution / License
The PCB design and schematic uses [Jerry Dunmire's ESP8266 KiCAD library](https://github.com/jdunmire/kicad-ESP8266) in the `hardware/libraries/ESP8266` directory, licensed under CC BY-SA 4.0. I license my hardware designs under the [CC BY-SA 4.0 license](https://creativecommons.org/licenses/by-sa/4.0/).

The ESP8266 software uses [Spritetm's libesphttpd with espfs](https://github.com/Spritetm/libesphttpd) in `esp8266/libesphttpd`. All my code is licensed under the [MIT license](http://opensource.org/licenses/MIT).
