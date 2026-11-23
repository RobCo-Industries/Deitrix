#!/usr/bin/env python3
"""Host test for tools/pbm2c.py. Run by make test-host."""

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
TOOL = os.path.join(ROOT, "tools", "pbm2c.py")
sys.path.insert(0, os.path.join(ROOT, "tools"))

import pbm2c  # noqa: E402


class Parse(unittest.TestCase):
    def test_plain(self):
        w, h, rows = pbm2c.parse("P1\n# c\n3 2\n1 0 1\n0 1 0\n")
        self.assertEqual((w, h), (3, 2))
        self.assertEqual(rows, [[1, 0, 1], [0, 1, 0]])

    def test_packed_digits_and_comments(self):
        w, h, rows = pbm2c.parse("P1 2 2 # size\n10\n# row\n01")
        self.assertEqual(rows, [[1, 0], [0, 1]])

    def test_errors(self):
        for bad in ["P4\n1 1\n1", "P1\n", "P1\nx 1\n1", "P1\n2 1\n1", "P1\n1 1\n2",
                    "P1\n0 1\n", "P1\n321 1\n" + "0" * 321]:
            with self.assertRaises(pbm2c.PbmError, msg=bad):
                pbm2c.parse(bad)


class Pack(unittest.TestCase):
    def test_msb_first_and_padding(self):
        stride, data = pbm2c.pack(10, [[1, 0, 0, 0, 0, 0, 0, 1, 1, 1]])
        self.assertEqual(stride, 2)
        self.assertEqual(data, [0x81, 0xC0])

    def test_rows(self):
        stride, data = pbm2c.pack(3, [[0, 1, 0], [1, 1, 1]])
        self.assertEqual((stride, data), (1, [0x40, 0xE0]))


class Cli(unittest.TestCase):
    def run_tool(self, files):
        with tempfile.TemporaryDirectory() as d:
            paths = []
            for name, text in files.items():
                p = os.path.join(d, name)
                with open(p, "w") as f:
                    f.write(text)
                paths.append(p)
            out = os.path.join(d, "gen", "icons.c")
            r = subprocess.run([sys.executable, TOOL, out] + paths,
                               capture_output=True, env=minimal_env(), text=True)
            c = open(out).read() if os.path.exists(out) else None
            return r, c

    def test_generates_sorted_table(self):
        r, c = self.run_tool({"zed.pbm": "P1 2 1 11", "abc.pbm": "P1 1 1 1"})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertLess(c.index('"abc"'), c.index('"zed"'))
        self.assertIn('{ "zed", 2, 1, 1, zed_bits }', c)
        self.assertIn("static const uint8_t zed_bits[1] = {\n    0xc0,\n};", c)
        self.assertIn("const int dx_icon_count = 2;", c)

    def test_bad_file_fails_without_output(self):
        r, c = self.run_tool({"bad.pbm": "P1 2 2 1"})
        self.assertEqual(r.returncode, 1)
        self.assertIn("bad.pbm", r.stderr)
        self.assertIsNone(c)

    def test_bad_name(self):
        r, c = self.run_tool({"Bad-Name.pbm": "P1 1 1 1"})
        self.assertEqual(r.returncode, 1)
        self.assertIsNone(c)

    def test_assets_are_valid(self):
        adir = os.path.join(ROOT, "assets")
        names = sorted(n for n in os.listdir(adir) if n.endswith(".pbm"))
        self.assertGreaterEqual(len(names), 3)
        for n in names:
            with open(os.path.join(adir, n)) as f:
                w, h, rows = pbm2c.parse(f.read())
            self.assertIn((w, h), [(16, 16), (24, 24)], n)
            self.assertTrue(any(any(r) for r in rows), n)


if __name__ == "__main__":
    unittest.main(verbosity=1)
