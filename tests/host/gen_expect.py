#!/usr/bin/env python3
"""Writes C fixtures for the host tests from the Python reference tools.

    tests/host/gen_expect.py <out.h>

- TUNES: each sample tune of content/ and a set of edge cases, with the
  notes tools/tun2notes.py makes of it, or the line of its error.
- BROKEN: each broken tree of tests/tools/test_check_content.py, as the
  files it changes. The C test loads each through the ROM's loaders.

Standard library only.
"""

import filecmp
import os
import shutil
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "tests", "tools"))

import tun2notes  # noqa: E402
import test_check_content as tcc  # noqa: E402

D = os.path.join("PIPBOY", "DATA")
RADIO = os.path.join(ROOT, "content", D, "RADIO")
PCM_SIZE = 256000

# Edge cases beyond the sample tunes: each runs through both parsers.
EXTRA_TUNES = [
    b"version 1\nA4 C4 C8 B8 C1 F#4 Gb4 R\n",
    b"version 1\nCb5 B#4 Cb1 B#8\n",
    b"version 1\n1000hz/8 20hz 20000hz 19hz\n",
    b"version 1\n1000hz/8 20hz 20000hz\n",
    b"version 1\ntempo 100 length 8 C4 C4/4. C4/1 tempo 300 C4/32 C4/32.\n",
    b"version 1\nC4 |: D4 E4 :| F4\n",
    b"version 1\nC4 D4 :| E4\n",
    b"; hi\r\nversion 1 ; v\r\nC#4 ; sharp\r\n",
    b"",
    b"\n\n; only a comment\n",
    b"version 2\nC4\n",
    b"version 1 extra\nC4\n",
    b"version 1\nversion 1\n",
    b"version 1\nC4 |: D4\n",
    b"version 1\n|: :|\n",
    b"version 1\nC4 :| :|\n",
    b"version 1\n|: C4 :| |: D4 :|\n",
    b"version 1\ntempo\n",
    b"version 1\ntempo 29 C4\n",
    b"version 1\ntempo 301 C4\n",
    b"version 1\nlength 3 C4\n",
    b"version 1\nC4/3\n",
    b"version 1\nC4.\n",
    b"version 1\nC4/4..\n",
    b"version 1\nc4\n",
    b"version 1\nH4\n",
    b"version 1\nC9\n",
    b"version 1\nC0\n",
    b"version 1\nCb1\n",
    b"version 1\nB#8\n",
    b"version 1\nC#\n",
    b"version 1\nR/16. R\n",
    b"version 1\nhz\n",
    b"version 1\n123456hz\n",
    b"version 1\n\tC4\tD4 \n",
    b"version 1\nC4 #comment\n",
    b"version 1\nC4\x01\n",
    b"version 1\nC4\xc3\n",
    b"version 1\n" + b"C4 " * 42 + b"C4\n",
    b"version 1\n" + b"C4 " * 43 + b"\n",
    b"version 1\nlength 32\n" + b"C4 D4 E4 F4\n" * 64,
    b"version 1\nlength 32\n" + b"C4 D4 E4 F4\n" * 64 + b"G4\n",
    b"version 1\nlength 32\n|:\n" + b"C4 D4 E4 F4\n" * 32 + b":|\n",
    b"version 1\nlength 32\n|:\n" + b"C4 D4 E4 F4\n" * 33 + b":|\n",
    b"version 1\nlength 32\n" + b"C4 D4 E4 F4\n" * 40 + b":|\nC4\n",
    b"version 1\ntempo 30\nC4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1.\n",
    b"version 1\ntempo 30\nC4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1. C4/1.\n",
    b"version 1\nC4\nversion\n",
    b"version 1\nC4\n\n",
    b"version 1\nC4",
    b"version 1\r\nC4\r",
]


def c_bytes(data):
    """A C string literal for any bytes, split so hex escapes end."""
    out = []
    for b in data:
        if b == 0x5C:
            out.append("\\\\")
        elif b == 0x22:
            out.append('\\"')
        elif b == 0x0A:
            out.append('\\n"\n    "')
        elif 0x20 <= b <= 0x7E and b != 0x3F:
            out.append(chr(b))
        else:
            out.append(f'\\x{b:02x}""')
    return '"' + "".join(out) + '"'


def tunes():
    cases = []
    for name in sorted(os.listdir(RADIO)):
        if name.endswith(".TUN"):
            with open(os.path.join(RADIO, name), "rb") as f:
                cases.append((name, f.read()))
    for i, data in enumerate(EXTRA_TUNES):
        cases.append((f"extra {i}", data))
    lines = ["static const tune_case_t TUNES[] = {"]
    notes = []
    for name, data in cases:
        try:
            got = tun2notes.parse(data)
            err = -1
        except tun2notes.TunError as e:
            got = []
            err = e.line
        start = len(notes)
        notes.extend(got)
        lines.append(f"    {{ {c_bytes(name.encode())}, {c_bytes(data)}, {len(data)}, {err}, "
                     f"{start}, {len(got)} }},")
    lines.append("};")
    table = ["static const uint16_t TUNE_NOTES[][2] = {"]
    table += [f"    {{ {f}, {d} }}," for f, d in notes] or ["    { 0, 0 },"]
    table.append("};")
    return "\n".join(table + lines)


def snapshot(tree):
    files = {}
    for dirpath, _, names in os.walk(tree):
        for n in names:
            p = os.path.join(dirpath, n)
            files[os.path.relpath(p, os.path.join(tree, D))] = p
    return files


def broken():
    out = ["static const broken_case_t BROKEN[] = {"]
    with tempfile.TemporaryDirectory() as d:
        base = os.path.join(d, "base")
        shutil.copytree(os.path.join(ROOT, "content"), base)
        for name in ("HEARTH.PCM", "DRIFT.PCM"):
            with open(os.path.join(base, D, "RADIO", name), "wb") as f:
                f.write(bytes([128]) * PCM_SIZE)
        before = snapshot(base)
        for what, rel, how, _ in tcc.BROKEN:
            tree = os.path.join(d, "t")
            shutil.rmtree(tree, ignore_errors=True)
            shutil.copytree(base, tree)
            tcc.apply(tree, rel, how)
            after = snapshot(tree)
            changes = []
            for r in sorted(set(before) | set(after)):
                card = "/" + "/".join([*D.split(os.sep), *r.split(os.sep)])
                if r not in after:
                    changes.append(f'{{ "{card}", NULL, -1, 0, 0 }}')
                elif r in before and filecmp.cmp(before[r], after[r], shallow=False):
                    continue
                else:
                    with open(after[r], "rb") as f:
                        data = f.read()
                    if r.endswith(".PCM"):
                        changes.append(f'{{ "{card}", NULL, {len(data)}, {data[0]}, {data[-1]} }}')
                    else:
                        changes.append(f'{{ "{card}", {c_bytes(data)}, {len(data)}, 0, 0 }}')
            assert 0 < len(changes) <= 32, (what, len(changes))
            out.append(f"    {{ {c_bytes(what.encode())}, {len(changes)}, {{\n        "
                       + ",\n        ".join(changes) + " } },")
    out.append("};")
    return "\n".join(out)


def main():
    text = ("/* Generated by tests/host/gen_expect.py from tools/tun2notes.py and\n"
            " * tests/tools/test_check_content.py. Do not edit. */\n"
            "#include <stddef.h>\n#include <stdint.h>\n\n"
            "typedef struct tune_case {\n    const char *name;\n    const char *data;\n"
            "    int size;\n    int err_line; /* -1: the tool accepts it */\n"
            "    int first, count;   /* in TUNE_NOTES */\n} tune_case_t;\n\n"
            "/* One changed file: data NULL with size -1 removes it; data NULL with\n"
            " * a size is a PCM file of that size, all `first` but its last byte. */\n"
            "typedef struct broken_file {\n    const char *path;\n    const char *data;\n"
            "    int size;\n    int first, last;\n} broken_file_t;\n\n"
            "typedef struct broken_case {\n    const char *what;\n    int count;\n"
            "    broken_file_t files[32];\n} broken_case_t;\n\n"
            + tunes() + "\n\n" + broken() + "\n")
    with open(sys.argv[1], "w") as f:
        f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
