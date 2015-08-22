# AVR-GCC Settings
MCU			:= atmega128a
F_CPU		:= 8000000
CC			:= avr-gcc
OBJCOPY		:= avr-objcopy
CFLAGS		:= -g -mmcu=$(MCU) -Wall -DF_CPU=$(F_CPU) -O
TARGET		:= build

# AVRDUDE Settings
PROGRAMMER	:= stk500v2
PORT		:= /dev/ttyACM0
FLASHFLAGS	:=
FLASHMCU	:= $(MCU)
#!!! Special case: avrdude doesn't know atmega128a, use atmega128!!!
FLASHMCU	:= atmega128

# Directory settings
BUILDDIR	:= build/
SRCDIR		:= src/
OBJDIR		:= obj/

SRCS		:= $(wildcard  $(SRCDIR)*.c)
OBJS		:= $(addprefix $(OBJDIR),$(notdir $(SRCS:.c=.o)))
OBJS		:= $(OBJS:.c=.o)

all: $(OBJDIR) $(BUILDDIR) $(BUILDDIR)$(TARGET).hex
	@echo Compilation succesfull. Type \"make flash\" to flash the firmware.

# ELF --> HEX
$(BUILDDIR)$(TARGET).hex: $(BUILDDIR)$(TARGET).elf
	$(OBJCOPY) -R .eeprom -O ihex $(BUILDDIR)$(TARGET).elf $(BUILDDIR)$(TARGET).hex

# Link .o files to .elf
$(BUILDDIR)$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) -o $(BUILDDIR)$(TARGET).elf $^

# Compile .c files to .o
$(OBJDIR)%.o: $(SRCDIR)%.c
	$(CC) $(CFLAGS) -o $@ -c $<

# Make build and object directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Clean directories
clean:
	$(RM) -r $(OBJDIR)
	$(RM) -r $(BUILDDIR)
	$(RM) *~
	$(RM) */*~

# Flash firmware to avr using avrdude
flash: all
	avrdude -c$(PROGRAMMER) -p$(FLASHMCU) $(FLASHFLAGS) -U flash:w:$(BUILDDIR)$(TARGET).hex -P $(PORT)
