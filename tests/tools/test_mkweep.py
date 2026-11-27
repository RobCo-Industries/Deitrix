"""Tests for tools/mkweep.py. Run by make tools-test."""

import copy
import hashlib
import json
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

import mkweep  # noqa: E402

TOOL = os.path.join(ROOT, "tools", "mkweep.py")
RATE = 37500

with open(mkweep.PARAMS, "r", encoding="utf-8") as f:
    PARAMS = json.load(f)
TABLE = mkweep.build(PARAMS)

# The SHA-256 of the table. Update it when the parameters change on purpose.
SHA256 = "5f5506c2e790476c805f0f0262e4a5ef6f4b60accc3c58417ba50189b530ab05"


def run(*args):
    return subprocess.run([sys.executable, "-I", TOOL] + list(args), env=minimal_env(),
                          capture_output=True, text=True, timeout=60)


def glide_alone(g):
    """One glide as floats, with no partial and no edge."""
    g = copy.deepcopy(g)
    g["partials"] = []
    g["fade_in_ms"] = 0
    g["fade_out_ms"] = 0
    g["start_ms"] = 0
    acc = [0.0] * mkweep.samples_of_ms(g["duration_ms"], RATE)
    mkweep.add_glide(acc, g, RATE)
    return acc


def rises(v):
    """Upward zero crossings, as fractional sample positions."""
    out = []
    for i in range(1, len(v)):
        if v[i - 1] < 0.0 <= v[i]:
            out.append(i - 1 + -v[i - 1] / (v[i] - v[i - 1]))
    return out


class Table(unittest.TestCase):
    def test_size_and_ends(self):
        self.assertEqual(len(TABLE), 5250)
        self.assertEqual(TABLE[0], 128)
        self.assertEqual(TABLE[-1], 128)

    def test_bytes(self):
        self.assertEqual(hashlib.sha256(TABLE).hexdigest(), SHA256)

    def test_peak(self):
        peak = max(abs(b - 128) for b in TABLE)
        # The three parts in phase add to 66.9 + 21.1 + 17.6 = 105.6.
        self.assertLessEqual(peak, 106)
        self.assertGreaterEqual(peak, 67)

    def test_partial_level(self):
        bright = PARAMS["glides"][0]
        self.assertEqual(bright["peak_steps"], 66.9)
        self.assertEqual(bright["partials"], [{"harmonic": 2, "peak_steps": 21.1}])
        # 10 dB under: the ratio of the peaks is 10^(10/20), 3.162.
        ratio = bright["peak_steps"] / bright["partials"][0]["peak_steps"]
        self.assertAlmostEqual(ratio, 3.1623, delta=0.01)
        self.assertEqual(PARAMS["glides"][1]["peak_steps"], 17.6)

    def test_edges(self):
        # Only the bright glide sounds in the first 10 ms: its peaks add
        # to 88 steps, which the 10 ms edge scales from 0.
        edge_in = 10 * RATE / 1000.0
        for i in range(int(edge_in)):
            self.assertLessEqual(abs(TABLE[i] - 128), 88.0 * i / edge_in + 1, i)
        # Both glides end with the table, each with a 15 ms edge.
        edge_out = 15 * RATE / 1000.0
        n = len(TABLE)
        for i in range(n - int(edge_out), n):
            left = n - 1 - i
            self.assertLessEqual(abs(TABLE[i] - 128), 105.6 * left / edge_out + 1, i)
        # Between the edges the weep is at full level.
        self.assertGreaterEqual(max(abs(b - 128) for b in TABLE[1000:4000]), 67)

    def test_pitches(self):
        for g in PARAMS["glides"]:
            v = glide_alone(g)
            n = len(v)
            up = rises(v)

            def curve(t):
                return g["start_hz"] * (g["end_hz"] / g["start_hz"]) ** (t / (n - 1))

            # One period at each end, against the curve at its middle.
            for a, b in ((up[0], up[1]), (up[-2], up[-1])):
                hz = RATE / (b - a)
                self.assertAlmostEqual(hz / curve((a + b) / 2), 1.0, delta=0.005, msg=g)
            # And the ends themselves, within the glide over one period.
            self.assertAlmostEqual(RATE / (up[1] - up[0]) / g["start_hz"], 1.0, delta=0.04)
            self.assertAlmostEqual(RATE / (up[-1] - up[-2]) / g["end_hz"], 1.0, delta=0.01)

    def test_constant_ratio(self):
        # The pitch halfway in time is the geometric mean of the ends.
        g = PARAMS["glides"][0]
        up = rises(glide_alone(g))
        mid = len(glide_alone(g)) // 2
        k = min(range(1, len(up)), key=lambda j: abs(up[j] - mid))
        hz = RATE / (up[k] - up[k - 1])
        mean = (g["start_hz"] * g["end_hz"]) ** 0.5
        self.assertAlmostEqual(hz / mean, 1.0, delta=0.01)

    def test_under_glide_starts_late(self):
        g = copy.deepcopy(PARAMS)
        g["glides"] = g["glides"][:1]
        alone = mkweep.build(g)
        start = 10 * RATE // 1000
        self.assertEqual(alone[:start], TABLE[:start])
        self.assertNotEqual(alone[start:], TABLE[start:])

    def test_same_bytes_twice(self):
        with tempfile.TemporaryDirectory() as d:
            for k in ("a", "b"):
                r = run("--dump", os.path.join(d, k))
                self.assertEqual(r.returncode, 0, r.stderr)
            with open(os.path.join(d, "a", "weep.u8"), "rb") as f:
                a = f.read()
            with open(os.path.join(d, "b", "weep.u8"), "rb") as f:
                b = f.read()
        self.assertEqual(a, b)
        self.assertEqual(a, TABLE)


class Check(unittest.TestCase):
    def test_committed_table_is_current(self):
        r = run("--check")
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_stale_table_fails(self):
        with tempfile.TemporaryDirectory() as d:
            c = os.path.join(d, "weep_table.c")
            h = os.path.join(d, "weep_table.h")
            r = run("--out-c", c, "--out-h", h)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(run("--check", "--out-c", c, "--out-h", h).returncode, 0)
            with open(c, "a", encoding="utf-8") as f:
                f.write("/* edited */\n")
            r = run("--check", "--out-c", c, "--out-h", h)
            self.assertEqual(r.returncode, 1)
            self.assertIn("is stale", r.stderr)

    def test_changed_params_fail_the_check(self):
        p = copy.deepcopy(PARAMS)
        p["glides"][1]["peak_steps"] = 17.0
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "p.json")
            with open(path, "w", encoding="utf-8") as f:
                json.dump(p, f)
            r = run("--check", "--params", path)
        self.assertEqual(r.returncode, 1)

    def test_bad_params(self):
        for key, val in (("samples", 1), ("rate_hz", 44100), ("ceiling_steps", 128)):
            p = copy.deepcopy(PARAMS)
            p[key] = val
            with self.assertRaises(mkweep.ParamError, msg=key):
                mkweep.build(p)
        p = copy.deepcopy(PARAMS)
        p["glides"][0]["peak_steps"] = 90
        with self.assertRaises(mkweep.ParamError):
            mkweep.build(p)

    def test_a_glide_at_half_the_rate_is_refused(self):
        # With no partial too: the pitch itself would alias.
        for end in ("start_hz", "end_hz"):
            p = copy.deepcopy(PARAMS)
            p["glides"][1]["partials"] = []
            p["glides"][1][end] = 20000
            with self.assertRaisesRegex(mkweep.ParamError, "half the rate", msg=end):
                mkweep.build(p)
        p = copy.deepcopy(PARAMS)
        p["glides"][1]["partials"] = []
        p["glides"][1]["end_hz"] = 18000
        mkweep.build(p)


if __name__ == "__main__":
    unittest.main()
