"""Tests for the chunk files make levels-preview hands RobCo-Core's
audio_preview.py. Run by make tools-test, with no QEMU.

Deitrix hands a recorded station to Pip-OS one buffer at a time, so the
trace names each buffer by its own CRC-32. tools/mkpcm.py --chunks cuts
a track into those buffers. A small hand-written log plays two of them:
the tool must find both and write a WAV of the right length, with no
warning. A buffer that no chunk matches gives the tool's warning.
"""

import os
import subprocess
import sys
import tempfile
import unittest
import wave
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORE = os.path.join(ROOT, "external", "pip-os", "external", "robco-core")
sys.path.insert(0, os.path.join(CORE, "tests"))
from harness.env import minimal_env  # noqa: E402
sys.path.insert(0, os.path.join(ROOT, "tools"))

import mkpcm  # noqa: E402

PREVIEW = os.path.join(CORE, "tools", "audio_preview.py")
RATE = 8000
TAIL_MS = 300
START_MS = 100
CHUNK = mkpcm.CHUNK


def log(crc0, crc1):
    end_ms = START_MS + 2 * CHUNK * 1000 // RATE
    return ("@RC stage=pipos ev=audio op=pcm rate=%d len=%d vol=51 crc=%08x t_ms=%d gain=5120\n"
            "@RC stage=pipos ev=audio op=pcm_next len=%d crc=%08x t_ms=%d\n"
            "@RC stage=pipos ev=audio op=pcm_end n=%d last=%d t_ms=%d\n"
            % (RATE, CHUNK, crc0, START_MS, CHUNK, crc1, START_MS + CHUNK * 1000 // RATE,
               CHUNK, CHUNK - 1, end_ms))


def render(tmp, text):
    chunks = os.path.join(tmp, "chunks")
    mkpcm.write_chunks({"HEARTH": mkpcm.TRACKS["HEARTH"]()}, chunks)
    path = os.path.join(tmp, "serial.log")
    with open(path, "w", encoding="ascii") as f:
        f.write(text)
    out = os.path.join(tmp, "out.wav")
    r = subprocess.run([sys.executable, "-I", PREVIEW, path, out, "--pcm", chunks],
                       cwd=ROOT, env=minimal_env(), capture_output=True, text=True, timeout=150)
    return r, out


class LevelsPreviewTest(unittest.TestCase):
    def test_two_chunks_render_to_their_length(self):
        data = mkpcm.TRACKS["HEARTH"]()
        crc0 = zlib.crc32(data[:CHUNK])
        crc1 = zlib.crc32(data[CHUNK:2 * CHUNK])
        with tempfile.TemporaryDirectory() as tmp:
            r, out = render(tmp, log(crc0, crc1))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertNotIn("warning", r.stderr)
            with wave.open(out) as w:
                rate = w.getframerate()
                frames = w.getnframes()
                samples = w.readframes(frames)
        end_ms = START_MS + 2 * CHUNK * 1000 // RATE
        self.assertEqual(frames, ((end_ms + TAIL_MS) * rate + 999) // 1000)
        self.assertTrue(any(samples), "the play is silent")

    def test_unknown_chunk_warns(self):
        data = mkpcm.TRACKS["HEARTH"]()
        crc0 = zlib.crc32(data[:CHUNK])
        crc1 = zlib.crc32(bytes(CHUNK))
        with tempfile.TemporaryDirectory() as tmp:
            r, _ = render(tmp, log(crc0, crc1))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("warning", r.stderr)


if __name__ == "__main__":
    unittest.main()
