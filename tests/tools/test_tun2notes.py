"""Tests for tools/tun2notes.py. Run by make tools-test."""

import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# RobCo-Core's minimal_env: subprocesses get an allowlisted environment,
# never a copy of the host one.
sys.path.insert(0, os.path.join(ROOT, "external", "pip-os", "external", "robco-core", "tests"))
from harness.env import minimal_env  # noqa: E402
RADIO = os.path.join(ROOT, "content", "PIPBOY", "DATA", "RADIO")
sys.path.insert(0, os.path.join(ROOT, "tools"))

import tun2notes  # noqa: E402
from tun2notes import TunError, parse  # noqa: E402


def tun(body):
    return ("version 1\n" + body).encode()


class SampleTunes(unittest.TestCase):
    def test_inside_play_seq_limits(self):
        names = sorted(n for n in os.listdir(RADIO) if n.endswith(".TUN"))
        self.assertEqual(names, ["LULLABY.TUN", "MORNING.TUN", "WALK.TUN"])
        for name in names:
            with open(os.path.join(RADIO, name), "rb") as f:
                notes = parse(f.read())
            self.assertGreaterEqual(len(notes), 1, name)
            self.assertLessEqual(len(notes), 256, name)
            for freq, ms in notes:
                self.assertTrue(freq == 0 or 20 <= freq <= 20000, (name, freq))
                self.assertTrue(1 <= ms <= 60000, (name, ms))
                self.assertLess(freq, 65536)
            total = sum(ms for _, ms in notes)
            self.assertGreaterEqual(total, 8000, name)
            self.assertLessEqual(total, 30000, name)


class Expand(unittest.TestCase):
    def test_note_names(self):
        self.assertEqual(parse(tun("A4 C4 C8 B8 C1 F#4 Gb4 R")),
                         [(440, 500), (262, 500), (4186, 500), (7902, 500),
                          (33, 500), (370, 500), (370, 500), (0, 500)])

    def test_flat_and_sharp_cross_the_octave(self):
        self.assertEqual(parse(tun("Cb5 B#4")), [(494, 500), (523, 500)])

    def test_hz(self):
        self.assertEqual(parse(tun("1000hz/8")), [(1000, 250)])

    def test_lengths_tempo_and_dots(self):
        self.assertEqual(parse(tun("tempo 100 length 8 C4 C4/4. C4/1 tempo 300 C4/32")),
                         [(262, 300), (262, 900), (262, 2400), (262, 25)])

    def test_repeat(self):
        self.assertEqual(parse(tun("C4 |: D4 E4 :| F4")),
                         [(262, 500), (294, 500), (330, 500), (294, 500), (330, 500), (349, 500)])

    def test_repeat_from_start(self):
        self.assertEqual(len(parse(tun("C4 D4 :| E4"))), 5)

    def test_comments_and_crlf(self):
        self.assertEqual(parse(b"; hi\r\nversion 1 ; v\r\nC#4 ; sharp\r\n"), [(277, 500)])

    def test_errors(self):
        bad = {
            "C4": "first line",
            "version 2\nC4": "first line",
            "version 1": "no notes",
            "version 1\nH4": "bad note",
            "version 1\nC9": "outside C1 to B8",
            "version 1\nC0": "outside C1 to B8",
            "version 1\n10hz": "frequency 10 outside",
            "version 1\nC4/3": "length 3",
            "version 1\ntempo 20 C4": "tempo 20 outside",
            "version 1\ntempo": "needs a value",
            "version 1\n|: C4": "without ':|'",
            "version 1\n|: C4 :| |: D4 :|": "only one repeat",
            "version 1\n:|": "empty repeat",
            "version 1\n" + "C4 D4\n" * 129: "more than 256 notes",
            "version 1\n|:\n" + "C4 D4\n" * 65 + ":|": "after the repeat",
            "version 1\ntempo 30\n" + "C4/1\n" * 16: "above 120000",
            "version 1\nC4 version 1": "first line",
            "version 1\n" + "C4 " * 50: "above 127",
            "version 1\nC4 \x01": "printable",
        }
        for text, msg in bad.items():
            with self.assertRaises(TunError, msg=text) as cm:
                parse(text.encode())
            self.assertIn(msg, cm.exception.msg, text)

    def test_size_limit(self):
        with self.assertRaises(TunError) as cm:
            parse(tun("; " + "x" * 100 + "\n") * 40)
        self.assertIn("above 4096", cm.exception.msg)


class Cli(unittest.TestCase):
    def test_wav(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "T.TUN")
            with open(src, "wb") as f:
                f.write(tun("A4/8 R/8"))
            out = os.path.join(d, "t.wav")
            r = subprocess.run([sys.executable, tun2notes.__file__, src, "--wav", out],
                               capture_output=True, env=minimal_env(), text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(r.stdout, "440 250\n0 250\n")
            self.assertEqual(os.path.getsize(out), 44 + 4000)

    def test_bad_file(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "T.TUN")
            with open(src, "wb") as f:
                f.write(tun("Q4"))
            r = subprocess.run([sys.executable, tun2notes.__file__, src],
                               capture_output=True, env=minimal_env(), text=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn("T.TUN:2: bad note 'Q4'", r.stderr)


if __name__ == "__main__":
    unittest.main()
