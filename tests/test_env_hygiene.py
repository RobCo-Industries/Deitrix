"""No Deitrix test or tool may hold a copy of the host environment.

pytest prints arguments and locals of failing frames, so a full copy of
os.environ would print every secret of the host. Tests build subprocess
environments with RobCo-Core's harness.env.minimal_env instead. The
pattern is the one of Pip-OS's tests/test_env_hygiene.py.
"""

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
THIS = os.path.abspath(__file__)

FULL_COPY = re.compile(
    r"dict\(\s*os\.environ"
    r"|os\.environ\.copy\("
    r"|\*\*\s*os\.environ\b"
    r"|env\s*=\s*os\.environ\b"
    r"|=\s*os\.environ\s*$"
    r"|copy\.(deep)?copy\(\s*os\.environ"
    r"|(dict|Env)\(\s*os\.environ\.items\("
    r"|for\s+\w+\s*,\s*\w+\s+in\s+os\.environ\.items\(\)",
    re.M)

SKIP_DIRS = {"__pycache__", "build", ".venv", ".git", "external"}


def sources():
    for top in ("tests", "tools"):
        for d, dirs, files in os.walk(os.path.join(ROOT, top)):
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
            for f in files:
                if f.endswith((".py", ".sh")):
                    yield os.path.join(d, f)


def test_no_full_environment_copy():
    bad = []
    for path in sources():
        if path == THIS:
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            for n, line in enumerate(f, 1):
                if FULL_COPY.search(line):
                    bad.append(f"{os.path.relpath(path, ROOT)}:{n}")
    assert not bad, "copy of os.environ, use minimal_env(): " + ", ".join(bad)


def test_pattern_catches_the_old_forms():
    for line in ('e = dict(os.environ, A="1")', "e = os.environ.copy()",
                 "e = {**os.environ}", "run(x, env=os.environ)", "e = os.environ",
                 "e = {k: v for k, v in os.environ.items() if k not in drop}"):
        assert FULL_COPY.search(line), line
    assert not FULL_COPY.search('os.environ.get("DEITRIX_UPDATE_GOLDEN")')
