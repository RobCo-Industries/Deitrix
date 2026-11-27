"""Tests for make check-workflows. Run by make tools-test."""

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

TOOL = os.path.join(CORE, "tools", "check_workflow_exprs.py")

EXPR_IN_RUN = """name: bad
on: push
jobs:
  a:
    runs-on: ubuntu-latest
    steps:
      - run: echo ${{ github.ref_name }}
"""

TAG_PIN = """name: bad
on: push
jobs:
  a:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
"""


def make(*args):
    return subprocess.run(["make", "--no-print-directory", "-C", ROOT] + list(args),
                          env=minimal_env(), capture_output=True, text=True, timeout=120)


class CheckWorkflowsTest(unittest.TestCase):
    def test_workflows_pass(self):
        r = make("-s", "check-workflows")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_check_pins_runs_the_guard(self):
        r = make("-n", "check-pins")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("check_workflow_exprs.py", r.stdout)

    def test_bad_files_fail(self):
        for name, text in (("expr.yml", EXPR_IN_RUN), ("tag.yml", TAG_PIN)):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as d:
                path = os.path.join(d, name)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
                r = subprocess.run([sys.executable, "-I", TOOL, path], env=minimal_env(),
                                   capture_output=True, text=True, timeout=60)
                self.assertNotEqual(r.returncode, 0, r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main()
