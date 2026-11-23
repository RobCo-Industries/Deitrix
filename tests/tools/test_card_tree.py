"""Tests for make card-tree. Run by make tools-test.

Each test builds into its own BUILD folder, so build/ is not touched.
"""

import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RADIO = os.path.join("card", "PIPBOY", "DATA", "RADIO")


def make(build, *args):
    return subprocess.run(["make", "--no-print-directory", "-C", ROOT, "BUILD=" + build,
                           "card-tree", *args], capture_output=True, text=True)


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


if __name__ == "__main__":
    unittest.main()
