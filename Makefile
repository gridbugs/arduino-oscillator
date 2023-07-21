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

periods.h : mk_periods.py
	python3 mk_periods.py > $@

%.o : %.c periods.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o *.elf

$(COMPILE_COMMANDS):
	$(TOOLS_DIR)/compile_commands_with_extra_include_paths.sh > $@

dev: $(COMPILE_COMMANDS)

dev-clean:
	rm -rf $(COMPILE_COMMANDS)

.PHONY: clean dev dev-clean
