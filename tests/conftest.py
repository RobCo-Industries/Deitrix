"""Target test setup: Pip-OS's pipos_testing package.

It comes from the Pip-OS submodule and loads RobCo-Core's harness, which
adds --board, the board fixture and need_mtools. `make test-target`
builds the kernel and the card first, into --build-dir (BUILD in the
Makefile).
"""

import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "external", "pip-os", "sdk", "testing"))
pytest_plugins = ["pipos_testing.plugin"]

import pytest  # noqa: E402


def serial_line(body):
    """A regex for whole serial lines that match body.

    The serial buffer can end in the middle of a line that is still
    arriving. With "$" and re.M a cut line matches at the end of the
    buffer, so a test can parse half a line. The pattern ends at the
    newline instead.
    """
    return re.compile("^" + body + r"\r?\n", re.M)


def pytest_addoption(parser):
    parser.addoption("--build-dir", default="build",
                     help="build directory, as BUILD in the Makefile")
    parser.addoption("--boot-sound-log", default=None,
                     help="test_boot_sound_log.py: where to save the serial log of the boot")
    parser.addoption("--sfx-preview-log", default=None,
                     help="test_sfx_preview_log.py: where to save the serial log of the session")
    parser.addoption("--levels-preview-log", default=None,
                     help="test_levels_preview_log.py: where to save the serial log of the session")
    parser.addoption("--levels-preview", default=None,
                     help="test_levels_preview_log.py: the levels of the session, such as default")


def pytest_ignore_collect(collection_path, config):
    """test_boot_sound_log.py, test_sfx_preview_log.py and
    test_levels_preview_log.py run only when they are given a log path,
    for make boot-sound, make sfx-preview and make levels-preview, so the
    suite neither skips them nor boots them."""
    if collection_path.name == "test_boot_sound_log.py" and not config.getoption("--boot-sound-log"):
        return True
    if collection_path.name == "test_sfx_preview_log.py" and not config.getoption("--sfx-preview-log"):
        return True
    if collection_path.name == "test_levels_preview_log.py" and not config.getoption("--levels-preview-log"):
        return True
    return None


def _built(request, *parts):
    path = os.path.join(ROOT, request.config.getoption("--build-dir"), *parts)
    if not os.path.exists(path):
        pytest.fail(f"{path} is missing: run make test-target, which builds it")
    return path


@pytest.fixture(scope="session")
def kernel_image(request):
    """The Pip-OS kernel image that make built from the submodule."""
    return _built(request, "pipos", "pipos.img")


@pytest.fixture(scope="session")
def holotest_kernel(request):
    """Pip-OS with its RAM card backend in the holotape slot, from make
    holotest-kernel. Only tests/test_slot.py boots it."""
    return _built(request, "holotest", "pipos.img")


@pytest.fixture
def card(request, tmp_path):
    """A copy of the card make sdimage wrote, so a test cannot change it."""
    path = tmp_path / "sd.img"
    shutil.copyfile(_built(request, "pipos", "sd.img"), path)
    return path


@pytest.fixture
def gallery_card(request, tmp_path):
    """A copy of the widget gallery's card, from make gallery."""
    path = tmp_path / "gallery.img"
    shutil.copyfile(_built(request, "gallery", "pipos", "sd.img"), path)
    return path

