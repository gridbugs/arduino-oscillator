#!/usr/bin/env python3

N_PERIODS = 512
START_NOTE_FREQUENCY_HZ = 27.5
OCTAVE_INDEX_RANGE = 64
FRAMES_PER_SECOND = 100000

def index_to_frequency_hz(index):
    return START_NOTE_FREQUENCY_HZ * (2 ** (index / OCTAVE_INDEX_RANGE))

def main():
    print("#include <stdint.h>")
    print("#define N_PERIODS %s" % N_PERIODS)
    print("const uint16_t periods[N_PERIODS] = {");
    for i in range(0, N_PERIODS):
        frequency_hz = index_to_frequency_hz(i)
        period_frames = FRAMES_PER_SECOND / frequency_hz
        print("    [%d] = %d, // %f Hz" % (i, round(period_frames), frequency_hz))
    print("};")

if __name__ == "__main__":
    main()
