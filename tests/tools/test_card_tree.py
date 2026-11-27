"""Tests for make card-tree. Run by make tools-test.

Each test builds into its own BUILD folder, so build/ is not touched.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# RobCo-Core's minimal_env: subprocesses get an allowlisted environment,
# never a copy of the host one.
sys.path.insert(0, os.path.join(ROOT, "external", "pip-os", "external", "robco-core", "tests"))
from harness.env import minimal_env  # noqa: E402
RADIO = os.path.join("card", "PIPBOY", "DATA", "RADIO")
NOTE = os.path.join("card", "PIPBOY", "VERSIONS.TXT")
NOTE_BYTES = b"Deitrix 0.1.0 0123456789abcdef\r\nPip-OS 7.1.0 fedcba9876543210\r\n"


def make(build, *args, target="card-tree"):
    return subprocess.run(["make", "--no-print-directory", "-C", ROOT, "BUILD=" + build,
                           target, *args], capture_output=True, env=minimal_env(), text=True)


def write_note(d):
    note = os.path.join(d, "note.txt")
    with open(note, "wb") as f:
        f.write(NOTE_BYTES)
    return note


class CardTree(unittest.TestCase):
    def test_missing_track_is_made_again(self):
        with tempfile.TemporaryDirectory() as d:
            r = make(d)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            os.remove(os.path.join(d, "pcm", "DRIFT.PCM"))
            r = make(d)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            for name in ("HEARTH.PCM", "DRIFT.PCM"):
                self.assertTrue(os.path.isfile(os.path.join(d, "pcm", name)), name)
                self.assertTrue(os.path.isfile(os.path.join(d, RADIO, name)), name)

    def test_missing_station_file_stops_the_build(self):
        with tempfile.TemporaryDirectory() as d:
            content = os.path.join(d, "content")
            shutil.copytree(os.path.join(ROOT, "content"), content)
            with open(os.path.join(content, "PIPBOY", "DATA", "RADIO.INI"), "a") as f:
                f.write("\n[station.6]\nname = Ghost\nkind = pcm\nfile = GHOST.PCM\n")
            r = make(os.path.join(d, "build"), "CONTENT=" + content)
            self.assertNotEqual(r.returncode, 0, r.stdout)
            self.assertIn("file 'GHOST.PCM' is not in RADIO/", r.stdout)


    def test_card_note_is_copied_as_versions_txt(self):
        with tempfile.TemporaryDirectory() as d:
            r = make(os.path.join(d, "build"), "CARD_NOTE=" + write_note(d))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            with open(os.path.join(d, "build", NOTE), "rb") as f:
                self.assertEqual(f.read(), NOTE_BYTES)

    def test_no_card_note_leaves_no_versions_txt(self):
        with tempfile.TemporaryDirectory() as d:
            build = os.path.join(d, "build")
            r = make(build, "CARD_NOTE=" + write_note(d))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            r = make(build)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertFalse(os.path.exists(os.path.join(build, NOTE)))

    def test_missing_card_note_stops_the_build(self):
        with tempfile.TemporaryDirectory() as d:
            r = make(os.path.join(d, "build"), "CARD_NOTE=" + os.path.join(d, "none.txt"))
            self.assertNotEqual(r.returncode, 0, r.stdout)
            self.assertIn("CARD_NOTE names no file", r.stderr)

    @unittest.skipUnless(shutil.which("mcopy") and shutil.which("arm-none-eabi-gcc"),
                         "needs mtools and the ARM toolchain")
    def test_sdimage_holds_the_note_only_when_asked(self):
        with tempfile.TemporaryDirectory() as d:
            build = os.path.join(d, "build")
            card = os.path.join(build, "pipos", "sd.img") + "@@1M"
            out = os.path.join(d, "got")
            env = dict(minimal_env(), MTOOLS_SKIP_CHECK="1")
            r = make(build, "CARD_NOTE=" + write_note(d), target="sdimage")
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            r = subprocess.run(["mdir", "-b", "-i", card, "::/PIPBOY"], capture_output=True,
                               env=env, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("::/PIPBOY/VERSIONS.TXT", r.stdout.split())
            r = subprocess.run(["mcopy", "-n", "-i", card, "::/PIPBOY/VERSIONS.TXT", out],
                               capture_output=True, env=env, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(out, "rb") as f:
                self.assertEqual(f.read(), NOTE_BYTES)
            r = make(build, target="sdimage")
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            r = subprocess.run(["mdir", "-b", "-i", card, "::/PIPBOY"], capture_output=True,
                               env=env, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertNotIn("VERSIONS.TXT", r.stdout)


if __name__ == "__main__":
    unittest.main()
