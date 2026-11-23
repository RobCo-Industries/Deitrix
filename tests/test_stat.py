"""The STAT tab in QEMU: its three sub-tabs, the keys and the empty state.

STAT logs a "deitrix: stat ..." line when a frame shows another view:
"shows ATTRS sel=<n> top=<n>", "shows STATUS" after a return, and on
ABOUT one "about" line per frame with every device value. The first
STATUS frame after the tab opens logs nothing. A line goes out during
the draw, just before the present, so a test takes its screenshot once
two in a row match.

ABOUT shows the SoC, the RAM and the uptime, which differ per board or
per run. Its goldens are two crops without those rows; the test reads
those values from the "about" line instead.
"""

import re
import time

from test_deitrix import boot_rom, golden, tabs, wait_ready, wait_tabs
from test_model import card_target, mtools
from pipos_testing import TIMEOUT, no_exception, press, screenshot, script
from conftest import serial_line

ATTRS = serial_line(r"deitrix: stat shows ATTRS sel=(\d+) top=(\d+)")
STATUS = serial_line(r"deitrix: stat shows STATUS")
ABOUT = serial_line(r"deitrix: stat about (.*?)")
SOC = {"raspi0": "BCM2835", "raspi1ap": "BCM2835", "raspi2b": "BCM2836", "raspi3b": "BCM2837"}
SAMPLE_ATTRS = 6

# Crops of the 640x480 screenshot, in rows of the 320x240 screen: 0 to
# 93 hold the header, the sub-tabs and the Deitrix, Pip-OS and ABI rows;
# 142 to 214 hold the Time row and the about lines.
ABOUT_TOP = (0, 0, 640, 2 * 94)
ABOUT_BOTTOM = (0, 2 * 142, 640, 2 * 215)


def count(vm, pattern):
    return len(pattern.findall(vm.serial))


def wait_count(vm, pattern, n):
    deadline = time.monotonic() + TIMEOUT
    while count(vm, pattern) < n:
        assert time.monotonic() < deadline, f"wanted {n} of {pattern.pattern}\n{vm.serial}"
        time.sleep(0.05)
    return pattern.findall(vm.serial)


def key(vm, name, pattern):
    """Taps a key, waits for one more line of pattern, returns the last."""
    before = count(vm, pattern)
    press(vm, name)
    return wait_count(vm, pattern, before + 1)[-1]


def settled(vm, tmp_path):
    """A screenshot once two taken 200 ms apart match: the frame whose
    line the test saw is then on screen."""
    last = screenshot(vm, tmp_path)
    deadline = time.monotonic() + TIMEOUT
    while True:
        time.sleep(0.2)
        img = screenshot(vm, tmp_path)
        if img.tobytes() == last.tobytes():
            return img
        assert time.monotonic() < deadline, "the screen did not settle"
        last = img


def about_fields(line):
    return dict(part.split("=", 1) for part in line.split(" | "))


def uptime_s(text):
    m = re.fullmatch(r"(\d+)d (\d\d):(\d\d):(\d\d)", text)
    assert m, text
    d, h, mi, s = (int(g) for g in m.groups())
    return ((d * 24 + h) * 60 + mi) * 60 + s


def quiet(vm, pattern):
    """No new line of pattern after a round trip through the kernel and
    a short wait: the key did nothing."""
    before = count(vm, pattern)
    script(vm, "state")
    time.sleep(0.3)
    assert count(vm, pattern) == before, vm.serial


def test_stat_walk(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        golden(settled(vm, tmp_path), "stat")
        # STATUS ignores UP, DOWN, SELECT and Back.
        for name in ("up", "down", "select", "back"):
            press(vm, name)
        quiet(vm, ATTRS)
        assert tabs(vm) == [], vm.serial

        # ATTRS: down to the last row, DOWN there does nothing, then up.
        assert key(vm, "right", ATTRS) == ("0", "0")
        golden(settled(vm, tmp_path), "stat_attrs")
        for i in range(1, SAMPLE_ATTRS):
            assert key(vm, "down", ATTRS) == (str(i), "0")
        press(vm, "down")
        press(vm, "select")
        quiet(vm, ATTRS)
        golden(settled(vm, tmp_path), "stat_attrs_end")
        for i in range(SAMPLE_ATTRS - 2, -1, -1):
            assert key(vm, "up", ATTRS) == (str(i), "0")
        press(vm, "up")
        quiet(vm, ATTRS)

        # ABOUT: the device rows, refreshed every second.
        line = key(vm, "right", ABOUT)
        f = about_fields(line)
        assert f["Deitrix"] == "0.1.0", line
        assert re.fullmatch(r"\d+\.\d+\.\d+", f["Pip-OS"]), line
        assert re.fullmatch(r"1\.\d+", f["ABI"]), line
        assert f["SoC"] == SOC[board], line
        assert re.fullmatch(r"[1-9]\d* MiB", f["RAM"]), line
        assert f["Time"] == "no clock", line
        first = uptime_s(f["Uptime"])
        n = count(vm, ABOUT)
        later = about_fields(wait_count(vm, ABOUT, n + 2)[-1])
        assert uptime_s(later["Uptime"]) - first in (2, 3), (line, later)
        img = settled_about(vm, tmp_path)
        golden(img.crop(ABOUT_TOP), "stat_about_top")
        golden(img.crop(ABOUT_BOTTOM), "stat_about_bottom")

        # RIGHT on the last sub-tab moves to INV; back on STAT, ABOUT stays.
        press(vm, "right")
        assert wait_tabs(vm, 1) == ["INV"], vm.serial
        n = count(vm, ABOUT)
        press(vm, "left")
        assert wait_tabs(vm, 2) == ["INV", "STAT"], vm.serial
        wait_count(vm, ABOUT, n + 1)
        # The tick stops on another sub-tab: no more about lines.
        assert key(vm, "left", ATTRS) == ("0", "0")
        n = count(vm, ABOUT)
        time.sleep(2.5)
        assert count(vm, ABOUT) == n, vm.serial
        before = count(vm, STATUS)
        press(vm, "left")
        wait_count(vm, STATUS, before + 1)
        golden(settled(vm, tmp_path), "stat")
        # LEFT on the first sub-tab of the first tab wraps to RADIO.
        press(vm, "left")
        assert wait_tabs(vm, 3) == ["INV", "STAT", "RADIO"], vm.serial
        no_exception(vm)


def settled_about(vm, tmp_path):
    """ABOUT redraws each second: wait for an about line, then shoot
    at once and keep the shot when the top and bottom crops hold still."""
    deadline = time.monotonic() + TIMEOUT
    while True:
        assert time.monotonic() < deadline, "ABOUT did not settle"
        a = screenshot(vm, tmp_path)
        time.sleep(0.2)
        b = screenshot(vm, tmp_path)
        if all(a.crop(c).tobytes() == b.crop(c).tobytes() for c in (ABOUT_TOP, ABOUT_BOTTOM)):
            return b


def test_stat_no_profile(board, kernel_image, card, tmp_path, need_mtools):
    mtools("mdel", "-i", card_target(card), "::PIPBOY/DATA/PROFILE.INI")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert "deitrix: load PROFILE.INI missing" in vm.serial, vm.serial
        golden(settled(vm, tmp_path), "stat_empty")
        assert key(vm, "right", ATTRS) == ("0", "0")
        golden(settled(vm, tmp_path), "stat_empty_attrs")
        press(vm, "down")
        quiet(vm, ATTRS)
        f = about_fields(key(vm, "right", ABOUT))
        assert f["Deitrix"] == "0.1.0" and f["SoC"] == SOC[board], vm.serial
        no_exception(vm)
