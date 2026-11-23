"""Tests for tools/mkpcm.py. Run by make tools-test."""

import hashlib
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
sys.path.insert(0, os.path.join(ROOT, "tools"))

import check_content  # noqa: E402
import mkpcm  # noqa: E402

# The SHA-256 of each track. The synthesis uses integers only, so these
# hold on every host. Update them when the score changes on purpose.
HASHES = {
    "HEARTH": "25491ed9d8356169f140b2ff63d640deca5d34dd5ad14b4593f63eb188ae1d6f",
    "DRIFT": "1b70a556566b08f7c401d5ba6b3be5a60dbc9a27d248b363e233ffd1a4c0f4e8",
}

TRACKS = {name: fn() for name, fn in mkpcm.TRACKS.items()}


class Tracks(unittest.TestCase):
    def test_hash(self):
        for name, data in TRACKS.items():
            self.assertEqual(hashlib.sha256(data).hexdigest(), HASHES[name], name)

    def test_deterministic(self):
        self.assertEqual(mkpcm.TRACKS["HEARTH"](), TRACKS["HEARTH"])

    def test_length(self):
        for name, data in TRACKS.items():
            self.assertGreaterEqual(len(data), 20 * mkpcm.RATE, name)
            self.assertLessEqual(len(data), 40 * mkpcm.RATE, name)
            self.assertLessEqual(len(data), check_content.PCM_MAX, name)

    def test_level(self):
        for name, data in TRACKS.items():
            self.assertGreater(min(data), 0, name)      # no clipping
            self.assertLess(max(data), 255, name)
            self.assertGreaterEqual(max(data) - min(data), 120, name)
            mean = sum(data) / len(data)
            self.assertLess(abs(mean - 128), 2, name)   # no DC offset

    def test_loop_clean(self):
        for name, data in TRACKS.items():
            steps = [abs(data[i + 1] - data[i]) for i in range(len(data) - 1)]
            joint = abs(data[-1] - data[0])
            self.assertLessEqual(joint, check_content.PCM_LOOP_STEP, name)
            # The joint is no larger than a step inside the track.
            self.assertLessEqual(joint, max(steps), name)
            self.assertLessEqual(max(steps), 24, name)


class Cli(unittest.TestCase):
    def test_writes_pcm_and_wav(self):
        with tempfile.TemporaryDirectory() as d:
            r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "mkpcm.py"), d,
                                "--wav", "--only", "DRIFT"], capture_output=True, env=minimal_env(), text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(sorted(os.listdir(d)), ["DRIFT.PCM", "DRIFT.WAV"])
            with open(os.path.join(d, "DRIFT.PCM"), "rb") as f:
                self.assertEqual(f.read(), TRACKS["DRIFT"])
            self.assertEqual(os.path.getsize(os.path.join(d, "DRIFT.WAV")), 44 + len(TRACKS["DRIFT"]))

    def test_list(self):
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "mkpcm.py"), "--list"],
                           capture_output=True, env=minimal_env(), text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.split(), [n + ".PCM" for n in TRACKS])


if __name__ == "__main__":
    unittest.main()
