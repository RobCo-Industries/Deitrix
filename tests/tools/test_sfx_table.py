"""Tests for the sound effect tables. Run by make tools-test.

The tables come from assets/sfx/sfx_models.json through RobCo-Core's
tools/sfx_table.py. These tests render them again, measure the dumped
tables with tools/sfx_check.py, and read make -n to see that the stale
check sits on every path to the ROM.
"""

import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORE = os.path.join(ROOT, "external", "pip-os", "external", "robco-core")
# RobCo-Core's minimal_env: subprocesses get an allowlisted environment,
# never a copy of the host one.
sys.path.insert(0, os.path.join(CORE, "tests"))
from harness.env import minimal_env  # noqa: E402

TOOL = os.path.join(CORE, "tools", "sfx_table.py")
CHECK = os.path.join(CORE, "tools", "sfx_check.py")
MODEL = os.path.join(ROOT, "assets", "sfx", "sfx_models.json")
OUT_C = os.path.join(ROOT, "src", "services", "sfx_table.c")
OUT_H = os.path.join(ROOT, "src", "services", "sfx_table.h")
TABLE_ARGS = [MODEL, "--c", OUT_C, "--h", OUT_H, "--prefix", "dx_sfx_", "--bed", "hum"]
NAMES = ["scroll", "select", "mode", "tab", "highlight", "tuner",
         "tape_start", "tape_stop", "hum_1", "hum_2"]


def run(args, **kw):
    return subprocess.run(args, cwd=ROOT, env=minimal_env(), capture_output=True,
                          text=True, timeout=150, **kw)


class SfxTableTest(unittest.TestCase):
    def test_committed_table_renders_again_byte_for_byte(self):
        r = run([sys.executable, "-I", TOOL] + TABLE_ARGS + ["--check", "--full"])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_dumped_tables_match_the_model(self):
        with tempfile.TemporaryDirectory() as d:
            r = run([sys.executable, "-I", TOOL, MODEL, "--dump", d])
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertEqual(sorted(os.listdir(d)), sorted(n + ".u8" for n in NAMES))
            r = run([sys.executable, "-I", CHECK, MODEL, d])
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_every_path_to_the_rom_runs_the_check(self):
        # -W makes the model look new, so the stamp is due and make -n
        # prints its recipe whatever the build folder holds.
        check = "sfx_table.py %s --c %s --h %s --prefix dx_sfx_ --bed hum --check" % (
            os.path.relpath(MODEL, ROOT), os.path.relpath(OUT_C, ROOT),
            os.path.relpath(OUT_H, ROOT))
        for goal in ("all", "sdimage", "run"):
            with self.subTest(goal=goal):
                r = run(["make", "-n", "-W", os.path.relpath(MODEL, ROOT), goal])
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                self.assertIn(check, r.stdout)
                # The check comes before the table builds and the ROM links.
                out = r.stdout
                self.assertLess(out.index(check), out.index("-o build/obj/src/services/sfx_table.c.o"))
                self.assertLess(out.index(check), out.index("-Map,build/ROM1.map"))


if __name__ == "__main__":
    unittest.main()
