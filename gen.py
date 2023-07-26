#!/usr/bin/env python3

import argparse
import math

N_PERIODS = 512
START_NOTE_FREQUENCY_HZ = 27.5
OCTAVE_INDEX_RANGE = 84
TICKS_PER_SECOND = 16000000

N_SAMPLES = 64
MAX_SAMPLE = 31

def constants():
    print("#ifndef _CONSTANTS")
    print("#define _CONSTANTS")
    print("#define N_SAMPLES %d" % N_SAMPLES)
    print("#endif")

def sine():
    print("#include <stdint.h>")
    constants()
    print("const uint8_t sine[N_SAMPLES] = {")
    for i in range(N_SAMPLES):
        sample = (MAX_SAMPLE * (math.sin((math.pi * 2 * i) / N_SAMPLES) + 1)) / 2
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
        period_ticks = TICKS_PER_SECOND / (frequency_hz * N_SAMPLES)
        print("    [%d] = %d, // %f Hz" % (i, round(period_ticks), frequency_hz))
    print("};")

def main(args):
    if args.output == "sine":
        sine()
    if args.output == "periods":
        periods()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        choices=["sine", "periods"],
        required=True,
    )
    args = parser.parse_args()
    main(args)
