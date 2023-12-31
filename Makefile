TARGET=main

SRC=main.c
OBJ=$(SRC:.c=.o)

CC=avr-gcc
MCU=atmega328p

# The --param=min-pagesize=0 argument is to fix the error:
# error: array subscript 0 is outside array bounds of ‘volatile uint8_t[0]’ {aka ‘volatile unsigned char[]’}
# ...which is incorrectly reported in some versions of gcc
CFLAGS=-mmcu=$(MCU) -std=c99 -O1 -Werror -Wall --param=min-pagesize=0 -Wno-error=unused-but-set-variable -Wno-error=unused-variable

TOOLS_DIR=tools
COMPILE_COMMANDS=compile_commands.json

$(TARGET).elf: $(OBJ) periods.h
	$(CC) $(CFLAGS) $(OBJ) -o $@

periods.h : gen.py
	python3 gen.py --output periods > $@

sine.h : gen.py
	python3 gen.py --output sine > $@

%.o : %.c low_res.h periods.h sine.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o *.elf periods.h sine.h

$(COMPILE_COMMANDS):
	$(TOOLS_DIR)/compile_commands_with_extra_include_paths.sh > $@

dev: $(COMPILE_COMMANDS)

dev-clean:
	rm -rf $(COMPILE_COMMANDS)

.PHONY: clean dev dev-clean
