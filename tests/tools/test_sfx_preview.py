"""Tests for the files make sfx-preview hands RobCo-Core's audio_preview.py.
Run by make tools-test, with no QEMU.

A small hand-written log plays one table. The tool must find the table
among the dumped .u8 files and write a WAV of the right length, with no
warning.
"""

import os
import re
import subprocess
import sys
import tempfile
import unittest
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORE = os.path.join(ROOT, "external", "pip-os", "external", "robco-core")
sys.path.insert(0, os.path.join(CORE, "tests"))
from harness.env import minimal_env  # noqa: E402

TABLE_TOOL = os.path.join(CORE, "tools", "sfx_table.py")
PREVIEW = os.path.join(CORE, "tools", "audio_preview.py")
MODEL = os.path.join(ROOT, "assets", "sfx", "sfx_models.json")
HEADER = os.path.join(ROOT, "src", "services", "sfx_table.h")
RATE = 37500
TAIL_MS = 300
START_MS = 100


def run(args):
    return subprocess.run(args, cwd=ROOT, env=minimal_env(), capture_output=True,
                          text=True, timeout=150)


def scroll():
    text = open(HEADER, encoding="ascii").read()
    length = int(re.search(r"#define DX_SFX_SCROLL_LEN (\d+)u", text).group(1))
    crc = re.search(r"#define DX_SFX_SCROLL_CRC32 0x([0-9a-f]+)u", text).group(1)
    return length, crc


class SfxPreviewTest(unittest.TestCase):
    def test_one_table_play_renders_to_its_length(self):
        length, crc = scroll()
        end_ms = START_MS + length * 1000 // RATE
        log = ("@RC stage=pipos ev=audio op=pcm rate=%d len=%d vol=100 crc=%s t_ms=%d\n"
               "@RC stage=pipos ev=audio op=pcm_end n=%d last=%d t_ms=%d\n"
               % (RATE, length, crc, START_MS, length, length - 1, end_ms))
        with tempfile.TemporaryDirectory() as tmp:
            tables = os.path.join(tmp, "sfx")
            r = run([sys.executable, "-I", TABLE_TOOL, MODEL, "--dump", tables])
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            path = os.path.join(tmp, "serial.log")
            with open(path, "w", encoding="ascii") as f:
                f.write(log)
            out = os.path.join(tmp, "out.wav")
            r = run([sys.executable, "-I", PREVIEW, path, out, "--pcm", tables])
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertNotIn("warning", r.stderr)
            with wave.open(out) as w:
                self.assertEqual(w.getframerate(), RATE)
                frames = w.getnframes()
                data = w.readframes(frames)
        # The play ends on the next whole millisecond, then the tail of
        # silence follows. A frame starts at or after each time.
        end_ms = START_MS + (length * 1000 + RATE - 1) // RATE
        self.assertEqual(frames, ((end_ms + TAIL_MS) * RATE + 999) // 1000)
        self.assertTrue(any(data), "the play is silent")


if __name__ == "__main__":
    unittest.main()
