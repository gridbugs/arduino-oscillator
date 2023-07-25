#include <stdio.h>
#include <avr/io.h>
#include <stdint.h>
#include "periods.h"
#include "sine.h"

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
    OCR1A = 32000; // initially reset after 2ms
    OCR1B = 0xFFFF;
}

void timer_set_output_compare(uint16_t value) {
    OCR1A = value;
}

int timer_match_check_and_clear(void) {
    if (TIFR1 & (1 << OCF1A)) {
        TIFR1 |= (1 << OCF1A);
        return 1;
    } else {
        return 0;
    }
}

void ADC_init(void) {
    PRR &= ~(1 << PRADC); // disable power reduction ADC bit
    ADCSRA = (1 << ADEN); // enable the ADC
    DIDR0 = 0; // enable all the digital IO pins with the ADC. We'll just use channels 6 and 7.
}

void ADC_set_channel(uint8_t channel) {
    ADMUX = (1 << REFS0) | channel;
}

void ADC_start_read(void) {
    ADCSRA |= (1 << ADSC); // start the conversion
}

uint16_t ADC_complete_read(void) {
    while ((ADCSRA & (1 << ADSC)) != 0); // wait for the start bit to clear
    uint16_t lo = (uint16_t)ADCL;
    uint16_t hi = (uint16_t)ADCH << 8;;
    return hi | lo;
}

uint16_t ADC_read(uint8_t channel) {
    ADC_set_channel(channel);
    ADC_start_read();
    return ADC_complete_read();
}

uint16_t ADC_read_discarding_first(uint8_t channel) {
    ADC_read(channel);
    return ADC_read(channel);
}

typedef struct {
    uint8_t sine;
    uint8_t triangle;
    uint8_t saw;
    uint8_t pulse;
} channels_t;

channels_t make_channels(void) {
    return (channels_t) {
        .sine = 0,
        .triangle = 0,
        .saw = 0,
        .pulse = 0,
    };
}

void write_channels(channels_t* channels) {
    // 2 bits unused, 5 bits from sine, 1 bit from triangle
    PORTD = (channels->sine << 2) | ((channels->triangle & 0x1) << 7) | ((channels->pulse & 0x2) >> 1);
    // 4 bits from triangle, 1 bit from pulse, 1 bit from saw
    PORTB = (channels->triangle >> 1) | ((channels->pulse & 0x1) << 4) | ((channels->saw & 0x1) << 5);
    // 4 bits from saw
    PORTC = channels->saw >> 1;
}

int main(void) {
    timer_init();
    ADC_init();
    USART0_init();
    printf("\r\nHello, World!\r\n");

    DDRD |= 0xFD; // leave PD1 for TX (but still take RX as a digital IO pin)
    DDRB |= 0x3F; // leave the top two bits as they don't have pins
    DDRC |= 0x0F; // only the bottom 4 pins are used

    uint16_t count = 0;
    uint16_t count_detuned = 0;

    uint16_t adc6 = ADC_read_discarding_first(6);
    uint16_t adc7 = ADC_read_discarding_first(7);

    channels_t channels = make_channels();
    channels_t channels_detuned = make_channels();
    channels_t channels_combined = make_channels();

    while (1) {
        while (!timer_match_check_and_clear());

        timer_set_output_compare(periods[adc7 >> 1]);

        channels.saw = (count / 2) % 32;
        channels_detuned.saw = (count_detuned / 2) % 32;

        if ((count & (1 << 5)) && channels.triangle > 0) {
            channels.triangle--;
        } else if (channels.triangle < 31) {
            channels.triangle++;
        }
        if ((count_detuned & (1 << 5)) && channels_detuned.triangle > 0) {
            channels_detuned.triangle--;
        } else if (channels_detuned.triangle < 31) {
            channels_detuned.triangle++;
        }

        channels.sine = sine[count % SINE_N_SAMPLES];
        channels_detuned.sine = sine[count_detuned % SINE_N_SAMPLES];

        uint16_t pwm_compare = adc6 >> 5;
        if (pwm_compare == 0) {
            pwm_compare = 1;
        }
        channels.pulse = (count % SINE_N_SAMPLES) < pwm_compare;
        channels_detuned.pulse = (count_detuned % SINE_N_SAMPLES) < pwm_compare;

        channels_combined.sine = (channels.sine + channels_detuned.sine) / 2;
        channels_combined.triangle = (channels.triangle + channels_detuned.triangle) / 2;
        channels_combined.saw = (channels.saw + channels_detuned.saw) / 2;
        channels_combined.pulse =  channels.pulse + channels_detuned.pulse;

        write_channels(&channels_combined);

        // Spread ADC interactions out over several frames. We don't care about
        // the latency of ADC updates and a single ADC read takes longer than
        // we can afford to spend mid-frame. Further, rapidly reading the ADC
        // after switching channels results in reading residual values from the
        // previous channel. This seems to be mitigated by doing a dummy read
        // and discarding its value before doing the "real" read after each
        // channel switch.
        switch (count & 0xFF) {
            case 0:
                ADC_set_channel(6);
                ADC_start_read();
                break;
            case 0x20:
                // discard first result since channel switch
                ADC_complete_read();
                ADC_start_read();
                break;
            case 0x40:
                adc6 = ADC_complete_read();
                break;
            case 0x60:
                ADC_set_channel(7);
                ADC_start_read();
                break;
            case 0x80:
                // discard first result since channel switch
                ADC_complete_read();
                ADC_start_read();
                break;
            case 0xA0:
                adc7 = ADC_complete_read();
                break;
        }
        //if ((count & 0xFF) == 0) {
        //    printf("%04d %04d\n\r", adc6, adc7);
        //}
        count += 1;
        if (count % 128 != 0) {
            count_detuned++;
        }
    }

    return 0;
}
