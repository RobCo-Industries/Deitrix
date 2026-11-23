#!/usr/bin/env python3
"""Expands a Deitrix .TUN tune into the notes the ROM sends to play_seq.

    tools/tun2notes.py <file.TUN>              print one "freq_hz duration_ms" per line
    tools/tun2notes.py <file.TUN> --wav <out>  also render it with a square wave

The format is in docs/CONTENT.md. Standard library only, Python 3.9+.
"""

import argparse
import sys
import wave

TUN_MAX_BYTES = 4096
TUN_MAX_LINE = 127
TUN_MAX_NOTES = 256
TUN_MAX_MS = 120000
TEMPO_MIN, TEMPO_MAX, TEMPO_DEFAULT = 30, 300, 120
LENGTHS = (1, 2, 4, 8, 16, 32)
LENGTH_DEFAULT = 4
FREQ_MIN, FREQ_MAX = 20, 20000

# Octave 8, C to B, in Hz. A lower octave shifts right with rounding.
OCTAVE8 = (4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902)
STEPS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


class TunError(Exception):
    def __init__(self, line, msg):
        super().__init__(f"{line}: {msg}")
        self.line = line
        self.msg = msg


def note_freq(letter, accidental, octave):
    """The frequency in Hz of a note name, as the ROM computes it."""
    step = STEPS[letter] + {"": 0, "#": 1, "b": -1}[accidental]
    if step < 0:
        step += 12
        octave -= 1
    elif step > 11:
        step -= 12
        octave += 1
    if octave < 1 or octave > 8:
        return None
    shift = 8 - octave
    f = OCTAVE8[step]
    return f if shift == 0 else (f + (1 << (shift - 1))) >> shift


def duration_ms(tempo, length, dotted):
    return 240000 * (3 if dotted else 2) // (tempo * length * 2)


def _pitch(tok, line):
    """Returns the frequency of a pitch token, 0 for a rest."""
    if tok == "R":
        return 0
    if tok.endswith("hz"):
        digits = tok[:-2]
        if not digits.isdigit() or len(digits) > 5:
            raise TunError(line, f"bad frequency '{tok}'")
        f = int(digits)
        if f < FREQ_MIN or f > FREQ_MAX:
            raise TunError(line, f"frequency {f} outside {FREQ_MIN} to {FREQ_MAX}")
        return f
    if len(tok) < 2 or tok[0] not in STEPS:
        raise TunError(line, f"bad note '{tok}'")
    acc = tok[1] if tok[1] in "#b" else ""
    octs = tok[1 + len(acc):]
    if len(octs) != 1 or not octs.isdigit():
        raise TunError(line, f"bad note '{tok}'")
    f = note_freq(tok[0], acc, int(octs))
    if f is None:
        raise TunError(line, f"note '{tok}' outside C1 to B8")
    return f


def _number(tok, line, what):
    if not tok.isdigit() or len(tok) > 5:
        raise TunError(line, f"bad {what} '{tok}'")
    return int(tok)


def parse(data):
    """Parses .TUN bytes. Returns the list of (freq_hz, duration_ms).

    Raises TunError with the line number for any rule in docs/CONTENT.md.
    """
    if len(data) > TUN_MAX_BYTES:
        raise TunError(0, f"file is {len(data)} bytes, above {TUN_MAX_BYTES}")
    tempo, length = TEMPO_DEFAULT, LENGTH_DEFAULT
    notes = []
    rep_start = None    # index in notes where |: opened
    rep_used = False
    seen_version = False
    lines = data.split(b"\n")
    if lines and lines[-1] == b"":
        lines.pop()
    for n, raw in enumerate(lines, 1):
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        if len(raw) > TUN_MAX_LINE:
            raise TunError(n, f"line is {len(raw)} bytes, above {TUN_MAX_LINE}")
        if any((b < 0x20 and b != 9) or b > 0x7E for b in raw):
            raise TunError(n, "byte outside printable ASCII")
        text = raw.decode("ascii").split(";", 1)[0]
        toks = text.split()
        if not toks:
            continue
        if not seen_version:
            if toks != ["version", "1"]:
                raise TunError(n, "the first line must be 'version 1'")
            seen_version = True
            continue
        i = 0
        while i < len(toks):
            tok = toks[i]
            if tok in ("tempo", "length"):
                if i + 1 >= len(toks):
                    raise TunError(n, f"'{tok}' needs a value")
                v = _number(toks[i + 1], n, tok)
                if tok == "tempo":
                    if v < TEMPO_MIN or v > TEMPO_MAX:
                        raise TunError(n, f"tempo {v} outside {TEMPO_MIN} to {TEMPO_MAX}")
                    tempo = v
                else:
                    if v not in LENGTHS:
                        raise TunError(n, f"length {v} is not 1, 2, 4, 8, 16 or 32")
                    length = v
                i += 2
                continue
            if tok == "version":
                raise TunError(n, "'version' only on the first line")
            if tok == "|:":
                if rep_used or rep_start is not None:
                    raise TunError(n, "only one repeat per tune")
                rep_start = len(notes)
                i += 1
                continue
            if tok == ":|":
                if rep_used:
                    raise TunError(n, "only one repeat per tune")
                start = rep_start if rep_start is not None else 0
                if start == len(notes):
                    raise TunError(n, "empty repeat")
                notes.extend(notes[start:])
                rep_start, rep_used = None, True
                i += 1
                continue
            pitch, _, dur = tok.partition("/")
            freq = _pitch(pitch, n)
            ln, dotted = length, False
            if "/" in tok:
                if dur.endswith("."):
                    dur, dotted = dur[:-1], True
                ln = _number(dur, n, "length")
                if ln not in LENGTHS:
                    raise TunError(n, f"length {ln} is not 1, 2, 4, 8, 16 or 32")
            notes.append((freq, duration_ms(tempo, ln, dotted)))
            if len(notes) > TUN_MAX_NOTES:
                raise TunError(n, f"more than {TUN_MAX_NOTES} notes")
            i += 1
    if not seen_version:
        raise TunError(1, "no 'version 1' line")
    if rep_start is not None:
        raise TunError(len(lines), "'|:' without ':|'")
    if not notes:
        raise TunError(len(lines), "no notes")
    if len(notes) > TUN_MAX_NOTES:
        raise TunError(len(lines), f"more than {TUN_MAX_NOTES} notes after the repeat")
    total = sum(d for _, d in notes)
    if total > TUN_MAX_MS:
        raise TunError(len(lines), f"plays {total} ms, above {TUN_MAX_MS}")
    return notes


def render_wav(notes, path, rate=8000):
    """Writes the notes as a square wave, 8-bit unsigned mono."""
    out = bytearray()
    for freq, ms in notes:
        n = rate * ms // 1000
        if freq == 0:
            out += bytes([128]) * n
            continue
        for i in range(n):
            out.append(176 if (2 * i * freq // rate) % 2 == 0 else 80)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(rate)
        w.writeframes(bytes(out))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("tune")
    ap.add_argument("--wav", help="render the tune to this WAV file")
    a = ap.parse_args(argv)
    with open(a.tune, "rb") as f:
        data = f.read()
    try:
        notes = parse(data)
    except TunError as e:
        print(f"{a.tune}:{e.line}: {e.msg}", file=sys.stderr)
        return 1
    for freq, ms in notes:
        print(freq, ms)
    print(f"# {len(notes)} notes, {sum(d for _, d in notes)} ms", file=sys.stderr)
    if a.wav:
        render_wav(notes, a.wav)
    return 0


if __name__ == "__main__":
    sys.exit(main())
