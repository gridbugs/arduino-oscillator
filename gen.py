#!/usr/bin/env python3

import argparse
import math

N_PERIODS = 512
START_NOTE_FREQUENCY_HZ = 27.5
OCTAVE_INDEX_RANGE = 84
TICKS_PER_SECOND = 16000000

SINE_N_SAMPLES = 64
SINE_MAX_SAMPLE = 31
def sine():
    print("#include <stdint.h>")
    print("#define SINE_N_SAMPLES %d" % SINE_N_SAMPLES)
    print("const uint8_t sine[SINE_N_SAMPLES] = {")
    for i in range(SINE_N_SAMPLES):
        sample = (SINE_MAX_SAMPLE * (math.sin((math.pi * 2 * i) / SINE_N_SAMPLES) + 1)) / 2
        print("    [%d] = %d," % (i, int(sample)))
    print("};")

def index_to_frequency_hz(index):
    return START_NOTE_FREQUENCY_HZ * (2 ** (index / OCTAVE_INDEX_RANGE))

def periods():
    print("#include <stdint.h>")
    print("#define N_PERIODS %d" % N_PERIODS)
    print("const uint16_t periods[N_PERIODS] = {");
    for i in range(0, N_PERIODS):
        frequency_hz = index_to_frequency_hz(i)
        period_ticks = TICKS_PER_SECOND / (frequency_hz * SINE_N_SAMPLES)
        print("    [%d] = %d, // %f Hz" % (i, round(period_ticks), frequency_hz))
    print("};")

def main(args):
    if args.output == "periods":
        periods()
    if args.output == "sine":
        sine()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        choices=["periods", "sine"],
        required=True,
    )
    args = parser.parse_args()
    main(args)
