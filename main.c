#include <stdio.h>
#include <avr/io.h>
#include "periods.h"

// The arduino clock is 16Mhz and the USART0 divides this clock rate by 16
#define USART0_CLOCK_HZ 1000000
#define BAUD_RATE_HZ 9600
#define UBRR_VALUE (USART0_CLOCK_HZ / BAUD_RATE_HZ)

// Send a character over USART0.
int USART0_tx(char data, struct __file* _f) {
    while (!(UCSR0A & (1 << UDRE0))); // wait for the data buffer to be empty
    UDR0 = data; // write the character to the data buffer
    return 0;
}

// Create a stream associated with transmitting data over USART0 (this will be
// used for stdout so we can print to a terminal with printf).
static FILE uartout = FDEV_SETUP_STREAM(USART0_tx, NULL, _FDEV_SETUP_WRITE);

void USART0_init(void) {
    UBRR0H = (UBRR_VALUE >> 8) & 0xF; // set the high byte of the baud rate
    UBRR0L = UBRR_VALUE & 0xFF; // set the low byte of the baud rate
    UCSR0B = 1 << TXEN0; // enable the USART0 transmitter
    UCSR0C = 3 << UCSZ00; // use 8-bit characters
    stdout = &uartout;
}

void timer_init(void) {
    // Put timer in CTC mode where the counter resets when it equals OCR1A
    TCCR1A &= ~((1 << WGM11) | (1 << WGM10));
    TCCR1B |= (1 << WGM12);
    TCCR1B &= ~(1 << WGM13);
    TCCR1B |= (1 << CS10);
    TCCR1B &= ~(1 << CS11);
    TCCR1B &= ~(1 << CS12);
    OCR1A = 160; // resets at 100khz
    OCR1B = 0xFFFF;
}


void ADC_init(void) {
    PRR &= ~(1 << PRADC); // disable power reduction ADC bit
    ADMUX = 0; // use AREF pin for reference voltage, right adjust the result, select ADC0 channel
    ADCSRA = (1 << ADEN); // enable the ADC
    DIDR0 = 0; // enable all the digital IO pins with the ADC. We'll just use channels 6 and 7.
}

void ADC_start_read(uint8_t channel) {
    // This also clears the control bits in ADMUX which should be 0 anyway.
    // Doing this in two stages (clearing the channel bits and then or-ing in
    // the new channel bits seems to blend the value at ADC0 with the intended
    // ADC channel so we do it in one stage by writing the channel directly to
    // the ADC.
    ADMUX = (channel & 0xF);
    ADCSRA |= (1 << ADSC); // start the conversion
}

uint16_t ADC_complete_read(void) {
    while ((ADCSRA & (1 << ADSC)) != 0); // wait for the start bit to clear
    uint16_t lo = (uint16_t)ADCL;
    uint16_t hi = (uint16_t)ADCH << 8;;
    return hi | lo;
}

uint16_t ADC_read(uint8_t channel) {
    ADC_start_read(channel);
    return ADC_complete_read();
}

int timer_match_check_and_clear(void) {
    if (TIFR1 & (1 << OCF1A)) {
        TIFR1 |= (1 << OCF1A);
        return 1;
    } else {
        return 0;
    }
}

int main(void) {
    timer_init();
    ADC_init();
    USART0_init();
    printf("\r\nHello, World!\r\n");

    while (1) {
        uint16_t adc6 = ADC_read(6);
        uint16_t adc7 = ADC_read(7);
        printf("%d %d\n\r", adc6, adc7);
    }

    return 0;
}
