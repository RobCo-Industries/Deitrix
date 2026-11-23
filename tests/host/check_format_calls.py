#!/usr/bin/env python3
"""Fail on a direct call to an unchecked libc string writer.

CI builds the host tests with GCC and -Werror. GCC's -Wformat-truncation
flags a snprintf whose output may not fit; the local Apple clang does
not, so such a call passes here and fails in CI. Formatted writes in
tests/host/ go through tfmt() (tests/host/tfmt.c, the one file allowed
to call vsnprintf). src/ has no libc formatter at all.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
ALLOWED = {ROOT / "tests/host/tfmt.c"}
CALL = re.compile(r"\b(v?s(?:n)?printf|strcpy|strcat|strncpy|strncat)\s*\(")
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def main() -> int:
    bad = []
    for top in ("src", "tests/host"):
        for path in sorted((ROOT / top).rglob("*")):
            if path.suffix not in (".c", ".h", ".inc") or path in ALLOWED:
                continue
            text = COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), path.read_text())
            for no, line in enumerate(text.splitlines(), 1):
                m = CALL.search(line)
                if m:
                    bad.append(f"{path.relative_to(ROOT)}:{no}: {m.group(1)}() (use tfmt)")
    for b in bad:
        print(b)
    if bad:
        print(f"check_format_calls: {len(bad)} unchecked call(s)")
        return 1
    print("check_format_calls: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
