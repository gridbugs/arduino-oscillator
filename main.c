#include <stdio.h>
#include <avr/io.h>
#include <stdint.h>
#include "low_res.h"
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
    DIDR0 = (1 << ADC5D); // disable digital pin 5
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

uint32_t xorshift32() {
    static uint32_t state = 0x12345678;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

typedef enum {
    SINE = 0,
    TRIANGLE,
    PULSE,
    SAW,
    SUPER_SAW,
    CLIPPED_SAW,
    CLIPPED_SUPER_SAW,
    NOISE,
} waveform_t;

#define ADC_INDEX_WAVEFORM 5
#define ADC_INDEX_EFFECT 6
#define ADC_INDEX_FREQUENCY 7

typedef struct {
    uint16_t waveform;
    uint16_t effect;
    uint16_t frequency;
} dials_t;

dials_t dials_create(void) {
    return (dials_t) {
      .waveform = ADC_read_discarding_first(ADC_INDEX_WAVEFORM),
      .effect = ADC_read_discarding_first(ADC_INDEX_EFFECT),
      .frequency = ADC_read_discarding_first(ADC_INDEX_FREQUENCY),
    };
}

uint16_t dials_period(dials_t* dials) {
    return periods[dials->frequency >> 1];
}

waveform_t dials_waveform(dials_t* dials) {
    return dials->waveform / 128;
}

void update_dials(dials_t* dials, uint16_t count) {
    // Spread ADC interactions out over several frames. We don't care about
    // the latency of ADC updates and a single ADC read takes longer than
    // we can afford to spend mid-frame. Further, rapidly reading the ADC
    // after switching channels results in reading residual values from the
    // previous channel. This seems to be mitigated by doing a dummy read
    // and discarding its value before doing the "real" read after each
    // channel switch.
    switch (count & 0xFF) {
        case 0:
            ADC_set_channel(ADC_INDEX_WAVEFORM);
            ADC_start_read();
            break;
        case 0x20:
            // discard first result since channel switch
            ADC_complete_read();
            ADC_start_read();
            break;
        case 0x40:
            dials->waveform = ADC_complete_read();
            break;
        case 0x60:
            ADC_set_channel(ADC_INDEX_EFFECT);
            ADC_start_read();
            break;
        case 0x80:
            // discard first result since channel switch
            ADC_complete_read();
            ADC_start_read();
            break;
        case 0xA0:
            dials->effect = ADC_complete_read();
            ADC_set_channel(ADC_INDEX_FREQUENCY);
            ADC_start_read();
            break;
        case 0xC0:
            // discard first result since channel switch
            ADC_complete_read();
            ADC_start_read();
            break;
        case 0xE0:
            dials->frequency = ADC_complete_read();
            break;
    }
}

uint8_t sample_sine(uint16_t count) {
    return sine[count % N_SAMPLES];
}

uint8_t sample_saw(uint16_t count) {
    return (count / 2) % 32;
}

uint8_t sample_clipped_saw(uint16_t count) {
    uint16_t double_saw = count % 64;
    if (double_saw < 16) {
        return 0;
    } else if (double_saw < 48) {
        return double_saw - 16;
    } else {
        return 31;
    }
}

uint8_t sample_pulse(uint16_t count, uint16_t effect) {
    uint16_t pwm_compare = 32 - (effect >> 5);
    if ((count % N_SAMPLES) < pwm_compare) {
        return 0;
    } else {
        return 31;
    }
}

uint8_t sample_triangle(uint16_t count, uint8_t current) {
    if ((count & (1 << 5)) && current > 0) {
        return current - 1;
    } else if (current < 31) {
        return current + 1;
    }
    return current;
}

uint8_t sample_noise(void) {
    return xorshift32();
}

typedef struct {
    uint16_t count;
    uint16_t detuned;
} count_t;

void count_update(count_t* count) {
    count->count += 1;
    if (count->count % 128 != 0) {
        count->detuned++;
    }
}

uint8_t next_sample(dials_t* dials, count_t* count, uint8_t current_sample) {
    waveform_t waveform = dials_waveform(dials);
    switch (waveform) {
        case SINE:
            return sample_sine(count->count);
        case TRIANGLE:
            return sample_triangle(count->count, current_sample);
        case PULSE:
            return sample_pulse(count->count, dials->effect);
        case SAW:
            return sample_saw(count->count);
        case SUPER_SAW:
            return (sample_saw(count->count) + sample_saw(count->detuned)) / 2;
        case CLIPPED_SAW:
            return sample_clipped_saw(count->count);
        case CLIPPED_SUPER_SAW:
            return (sample_clipped_saw(count->count) + sample_clipped_saw(count->detuned)) / 2;
        case NOISE:
            return sample_noise();
    }
    return 0;
}

uint8_t apply_low_res(uint8_t sample, uint16_t effect) {
    uint16_t low_res_index = effect / 256;
    switch (low_res_index) {
        case 0:
            return sample;
        case 1:
            return low_res_16[sample];
        case 2:
            return low_res_8[sample];
        case 3:
            return low_res_4[sample];
    }
    return 0;
}

void write_sample(uint8_t sample) {
    PORTC = sample;
}

void set_waveform_led(waveform_t waveform) {
    switch (waveform) {
        case SINE:
            PORTD = (1 << 2);
            PORTB = 0;
            break;
        case TRIANGLE:
            PORTD = (1 << 3);
            PORTB = 0;
            break;
        case PULSE:
            PORTD = (1 << 4);
            PORTB = 0;
            break;
        case SAW:
            PORTD = (1 << 5);
            PORTB = 0;
            break;
        case SUPER_SAW:
            PORTD = (1 << 6);
            PORTB = 0;
            break;
        case CLIPPED_SAW:
            PORTD = (1 << 7);
            PORTB = 0;
            break;
        case CLIPPED_SUPER_SAW:
            PORTD = 0;
            PORTB = (1 << 0);
            break;
        case NOISE:
            PORTD = 0;
            PORTB = (1 << 1);
            break;
    }
}

int main(void) {
    timer_init();
    ADC_init();
    USART0_init();
    printf("\r\nHello, World!\r\n");

    DDRD |= 0xFC;
    DDRB |= 0x03;
    DDRC |= 0x1F;

    count_t count = { 0 };

    dials_t dials = dials_create();

    uint8_t sample = 0;
    int first = 100;
    while (1) {
        while (!timer_match_check_and_clear());
        timer_set_output_compare(dials_period(&dials));
        sample = next_sample(&dials, &count, sample);
        write_sample(apply_low_res(sample, dials.effect));
        update_dials(&dials, count.count);
        if (first > 0) {
            // this delay is because otherwise the wrong LED briefly turns on when the arduino first starts up
            first--;
        } else {
            set_waveform_led(dials_waveform(&dials));
        }
        count_update(&count);
    }

    return 0;
}
