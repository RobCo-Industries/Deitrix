#!/usr/bin/env python3
"""Generates the two sample radio tracks of the default card.

    tools/mkpcm.py <outdir>               write HEARTH.PCM and DRIFT.PCM
    tools/mkpcm.py <outdir> --wav         also write HEARTH.WAV and DRIFT.WAV
    tools/mkpcm.py <outdir> --only HEARTH one track only
    tools/mkpcm.py --list                 print the file names, one per line

Output: raw 8-bit unsigned mono at 8000 Hz, no header, as docs/CONTENT.md
describes. The synthesis uses integers only, so the bytes are the same on
every host and Python 3.9+. Each note is written around the loop: a note
that runs past the end continues at the start. The low-pass filter runs
around the loop twice. So the last sample leads into the first one like
any other pair of samples, and the track loops without a click.
"""

import argparse
import os
import sys
import wave

RATE = 8000
PEAK = 96          # output swing around 128
MASK32 = 0xFFFFFFFF

# Octave 8, C to B, in milli-hertz. A lower octave halves it.
OCTAVE8_MHZ = (4186009, 4434922, 4698636, 4978032, 5274041, 5587652,
               5919911, 6271927, 6644875, 7040000, 7458620, 7902133)
STEPS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def phase_inc(name):
    """The 32-bit phase step per sample for a note name such as 'F#3'."""
    step = STEPS[name[0]] + (1 if name[1] == "#" else -1 if name[1] == "b" else 0)
    octave = int(name[-1])
    mhz = OCTAVE8_MHZ[step % 12] >> (8 - octave - step // 12)
    return (mhz << 32) // (RATE * 1000)


def wave_sine(x):
    """A parabolic sine from a 16-bit phase, -32768 to 32768."""
    t = x & 0x7FFF
    y = (t * (32768 - t)) >> 13
    return y if x < 0x8000 else -y


def wave_tri(x):
    return 2 * x - 32768 if x < 0x8000 else 32767 - 2 * (x - 0x8000)


VOICES = {"sine": wave_sine, "tri": wave_tri}


def envelope(i, length, attack, release, pluck):
    """Gain 0 to 1024 at sample i of a note of `length` samples."""
    if i < attack:
        g = 1024 * i // attack
    elif pluck:
        left = length - i
        g = 1024 * left * left // ((length - attack) * (length - attack))
    else:
        g = 1024
    if length - i < release:
        g = g * (length - i) // release
    return g


def add_note(acc, start, length, name, voice, level, attack, release, pluck=False, partial=0):
    """Mixes one note into acc, wrapping around the end of the loop."""
    n = len(acc)
    inc = phase_inc(name)
    fn = VOICES[voice]
    ph = 0
    ph2 = 0
    for i in range(length):
        v = fn(ph >> 16)
        if partial:
            v += partial * wave_sine(ph2 >> 16) >> 8
            ph2 = (ph2 + 2 * inc) & MASK32
        g = envelope(i, length, attack, release, pluck)
        acc[(start + i) % n] += v * g * level >> 18
        ph = (ph + inc) & MASK32


def add_wind(acc, start, length, level, seed):
    """Soft noise that swells in and out, from a fixed LCG."""
    n = len(acc)
    s = seed
    lp = 0
    half = length // 2
    for i in range(length):
        s = (s * 1103515245 + 12345) & 0x7FFFFFFF
        lp += (((s >> 15) & 0xFFFF) - 32768 - lp) >> 4
        g = i if i < half else length - i
        acc[(start + i) % n] += lp * g // half * level >> 10


def lowpass_loop(acc, shift):
    """A one-pole low-pass run twice around the loop: the result is periodic."""
    y = 0
    out = [0] * len(acc)
    for _ in range(2):
        for i, x in enumerate(acc):
            y += (x - y) >> shift
            out[i] = y
    return out


def to_u8(acc):
    peak = max(1, max(abs(v) for v in acc))
    return bytes(128 + v * PEAK // peak for v in acc)


def up2(name):
    """The same note two octaves up."""
    return name[:-1] + str(int(name[-1]) + 2)


def hearth():
    """Hearth: a warm pad, a bass and a slow arpeggio. 60 bpm, 8 bars of
    4/4, 32 seconds. C, Am, F, G, C, Em, F, G."""
    eighth = RATE * 60 // 60 // 2          # 4000 samples
    bar = 8 * eighth
    acc = [0] * (8 * bar)
    chords = [("C3", "E3", "G3", "C2"), ("A2", "C3", "E3", "A1"),
              ("F2", "A2", "C3", "F1"), ("G2", "B2", "D3", "G1"),
              ("C3", "E3", "G3", "C2"), ("G2", "B2", "E3", "E1"),
              ("F2", "A2", "C3", "F1"), ("G2", "B2", "D3", "G1")]
    arp = [0, 1, 2, 1, 0, 1, 2, 1]
    for b, (r, t, f, bass) in enumerate(chords):
        s = b * bar
        for name in (r, t, f):
            add_note(acc, s, bar + 2 * eighth, name, "sine", 300, 3000, 6000)
        add_note(acc, s, 4 * eighth, bass, "tri", 360, 200, 1500)
        add_note(acc, s + 4 * eighth, 4 * eighth, bass, "tri", 300, 200, 1500)
        tones = (r, t, f)
        for k, idx in enumerate(arp):
            add_note(acc, s + k * eighth, 2 * eighth, up2(tones[idx]), "sine", 150,
                     80, 400, pluck=True)
    return to_u8(lowpass_loop(acc, 2))


def drift():
    """Drift: a drone in D, soft wind and bell notes from a D minor
    pentatonic picked by a fixed seed. 75 bpm, 10 bars of 4/4, 32 seconds."""
    eighth = RATE * 60 // 75 // 2          # 3200 samples
    bar = 8 * eighth
    acc = [0] * (10 * bar)
    for b in range(10):
        s = b * bar
        add_note(acc, s, 2 * bar, "D2", "tri", 200, bar // 2, bar // 2)
        add_note(acc, s + bar // 2, 2 * bar, "A2", "sine", 220, bar // 2, bar // 2)
    for b in range(0, 10, 2):
        add_wind(acc, b * bar + bar // 3, 2 * bar, 80, 7 + b)
    scale = ["D4", "F4", "G4", "A4", "C5", "D5", "F5", "A5"]
    seed = 2024
    pos = 0
    last = 0
    while pos < 10 * 8:
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        r = seed >> 16
        step = (r % 5) - 2
        last = min(len(scale) - 1, max(0, last + (step or 1)))
        length = (2, 3, 4, 2)[r % 4]
        add_note(acc, pos * eighth, 6 * eighth, scale[last], "sine", 210, 40, 1600,
                 pluck=True, partial=60)
        pos += length
    return to_u8(lowpass_loop(acc, 2))


TRACKS = {"HEARTH": hearth, "DRIFT": drift}


def write_wav(path, data):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(RATE)
        w.writeframes(data)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("outdir", nargs="?")
    ap.add_argument("--list", action="store_true", help="print the .PCM names and exit")
    ap.add_argument("--wav", action="store_true", help="also write <NAME>.WAV")
    ap.add_argument("--only", choices=sorted(TRACKS), help="one track only")
    a = ap.parse_args(argv)
    if a.list:
        for name in TRACKS:
            print(name + ".PCM")
        return 0
    if a.outdir is None:
        ap.error("outdir is required")
    os.makedirs(a.outdir, exist_ok=True)
    for name, fn in TRACKS.items():
        if a.only and name != a.only:
            continue
        data = fn()
        tmp = os.path.join(a.outdir, name + ".PCM.tmp")
        with open(tmp, "wb") as f:
            f.write(data)
        os.replace(tmp, os.path.join(a.outdir, name + ".PCM"))
        if a.wav:
            write_wav(os.path.join(a.outdir, name + ".WAV"), data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
