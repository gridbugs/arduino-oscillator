# Arduino Oscillator

## Useful Commands

### Flash an Arduino Nano
```
make && sudo avrdude -P /dev/ttyUSB0 -c arduino -p m328p -U flash:w:main.elf
```

### Connect USB serial console
```
sudo picocom -b9600 /dev/ttyUSB0
```
