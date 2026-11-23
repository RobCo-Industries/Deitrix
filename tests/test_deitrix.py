"""Deitrix on Pip-OS in QEMU: the first screen, tab navigation and the clock.

Each test boots the card from make sdimage. The screen goldens in
tests/golden/ are QEMU screenshots, 640x480, the same on every board.
DEITRIX_UPDATE_GOLDEN=1 writes them again, through check_golden of
Pip-OS's pipos_testing.

Without an RTC device Pip-OS has no clock, so the header shows --:--
and the goldens of the first two tests do not depend on the time. The
clock tests add QEMU's ds1338 RTC with a fixed start time.
"""

import pathlib
import time

from pipos_testing import TIMEOUT, booted, check_golden, no_exception, screenshot, script
from conftest import serial_line

GOLDEN_DIR = pathlib.Path(__file__).resolve().parent / "golden"
TAB_LINE = serial_line(r"deitrix: tab=(\w+)")
READY_LINE = serial_line(r"deitrix: ready")
RTC_DEVICE = ["-device", "ds1338,bus=i2c-bus.1,address=0x68"]


def boot_rom(image, board, sd, extra_args=None):
    """Boots Pip-OS with the script channel. The ROM never exits, so the
    kernel prints ev=jump but never ev=ready: wait for jump, then for
    deitrix: ready with wait_ready."""
    return booted(image, board, sd=sd, wait="jump", extra_args=extra_args)


def golden(img, name):
    check_golden(img, GOLDEN_DIR / f"{name}.png", update_var="DEITRIX_UPDATE_GOLDEN")


def tabs(vm):
    return TAB_LINE.findall(vm.serial)


def wait_tabs(vm, count):
    """Waits until the ROM has logged `count` tab changes, returns them all."""
    deadline = time.monotonic() + TIMEOUT
    while len(tabs(vm)) < count:
        assert time.monotonic() < deadline, f"wanted {count} tab lines\n{vm.serial}"
        time.sleep(0.05)
    return tabs(vm)


def wait_ready(vm):
    vm.wait_for(READY_LINE, timeout=TIMEOUT)
    assert len(READY_LINE.findall(vm.serial)) == 1, vm.serial


def rom_running(vm):
    reply = script(vm, "state")
    assert "rom=1" in reply.lines[0].split(), reply


def test_first_screen(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert tabs(vm) == [], vm.serial
        golden(screenshot(vm, tmp_path), "stat")
        rom_running(vm)
        no_exception(vm)


# (script command, the tab it leads to). From STAT: the dial both ways,
# several detents, LEFT and RIGHT, and a wrap each way by dial and by key.
STEPS = [
    ("dial 1", "INV"),
    ("dial 1", "DATA"),      # INV takes RIGHT for its sub-tabs
    ("key left", "INV"),
    ("dial -2", "RADIO"),    # wraps back past STAT
    ("dial 1", "STAT"),      # wraps forward past RADIO
    ("key left", "RADIO"),   # wraps back by key
    ("key right", "STAT"),   # wraps forward by key
    ("dial 3", "MAP"),
    ("dial -7", "INV"),
]


def test_navigation(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        script(vm, STEPS[0][0])
        assert wait_tabs(vm, 1) == ["INV"], vm.serial
        golden(screenshot(vm, tmp_path), "inv")
        for i, (command, _) in enumerate(STEPS[1:], start=2):
            script(vm, command)
            wait_tabs(vm, i)
        assert tabs(vm) == [tab for _, tab in STEPS], vm.serial
        # Keys that do not move: no new line, and the ROM still runs.
        for command in ("key up", "key down", "key select", "dial 5"):
            script(vm, command)
        rom_running(vm)
        assert tabs(vm) == [tab for _, tab in STEPS], vm.serial
        no_exception(vm)


def header_clock(img):
    """The clock area of the header, x 272 to 315 and y 0 to 20 on the 320x240 screen."""
    return img.crop((544, 0, 632, 42))


def test_clock_shown(board, kernel_image, card, tmp_path, need_mtools):
    # 09:47:00 at QEMU start: the ROM is up long before 09:48.
    with boot_rom(kernel_image, board, sd=card,
                extra_args=RTC_DEVICE + ["-rtc", "base=2077-10-23T09:47:00"]) as vm:
        wait_ready(vm)
        golden(header_clock(screenshot(vm, tmp_path)), "clock_0947")
        no_exception(vm)


def test_clock_minute_change(board, kernel_image, card, tmp_path, need_mtools):
    """The time redraws when the minute changes.

    The ROM sleeps until the next whole minute by the clock's seconds, so
    it redraws at most about 1 s after the change. The test allows
    10 s, for a loaded host, then the wait for the screenshot.
    """
    start = time.monotonic()
    with boot_rom(kernel_image, board, sd=card,
                extra_args=RTC_DEVICE + ["-rtc", "base=2077-10-23T09:47:30"]) as vm:
        wait_ready(vm)
        ready_at = time.monotonic() - start
        first = header_clock(screenshot(vm, tmp_path))
        assert ready_at < 30, f"ready after {ready_at:.1f} s, past 09:48: rerun alone"
        golden(first, "clock_0947")
        # QEMU's RTC started before the timer above, so 09:48 comes
        # at most 30 s after `start`.
        deadline = start + 30 + 10
        while True:
            img = header_clock(screenshot(vm, tmp_path))
            if img.tobytes() != first.tobytes():
                break
            assert time.monotonic() < deadline, "the clock did not move to 09:48"
            time.sleep(0.2)
        golden(img, "clock_0948")
        no_exception(vm)
