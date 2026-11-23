"""The DATA tab in QEMU: NOTES, TAPES and CLOCK.

DATA logs a "deitrix: data ..." line when a frame shows another view,
just before the present:

    deitrix: data notes sel=<n> top=<n> count=<n>
    deitrix: data note <FILE> top=<n> lines=<n>
    deitrix: data tapes sel=<n> top=<n> count=<n> | <result line>
    deitrix: data clock <HH:MM:SS> <YYYY-MM-DD> <weekday>, or "none"
    deitrix: data setter field=<n> <YYYY-MM-DD HH:MM> | <message>

A launch logs "deitrix: data launch <FILE>" before the call and
"deitrix: data tape rc=<n>" after it. A test takes its screenshot once
two in a row match.
"""

import pathlib
import re
import subprocess
import time

import pytest

from test_deitrix import boot_rom, golden, rom_running, tabs, wait_ready, wait_tabs
from test_model import card_target, mtools, wait_line
from test_stat import count, key, quiet, settled, wait_count
from pipos_testing import TIMEOUT, minimal_env, no_exception, press, script
from conftest import serial_line

ROOT = pathlib.Path(__file__).resolve().parent.parent
TAPES = {"linetest": "LINETEST.HTP", "fault": "FAULT.HTP"}

NOTES = serial_line(r"deitrix: data notes sel=(\d+) top=(\d+) count=(\d+)")
NOTE = serial_line(r"deitrix: data note (\S+) top=(\d+) lines=(\d+)( cut)?")
TAPES_LINE = serial_line(r"deitrix: data tapes sel=(\d+) top=(\d+) count=(-?\d+) \| (.*?)")
TAPE_RC = serial_line(r"deitrix: data tape rc=(-?\d+)")
SAMPLE_NOTES = ["GARDEN01.TXT", "RECIPE01.TXT", "SHOP01.TXT", "WALK01.TXT"]


def to_data(vm, dial="dial 2"):
    """To DATA with the dial: two detents from STAT by default."""
    before = count(vm, NOTES)
    n = len(tabs(vm))
    script(vm, dial)
    assert wait_tabs(vm, n + 1)[-1] == "DATA", vm.serial
    return wait_count(vm, NOTES, before + 1)[-1]


def test_data_notes(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_data(vm) == ("0", "0", "4")
        golden(settled(vm, tmp_path), "data_notes")
        # UP at the top and Back at the top level do nothing.
        press(vm, "up")
        press(vm, "back")
        quiet(vm, NOTES)
        for i in range(1, 4):
            assert key(vm, "down", NOTES) == (str(i), "0", "4")
        press(vm, "down")
        quiet(vm, NOTES)
        assert key(vm, "up", NOTES) == ("2", "0", "4")

        # SELECT opens SHOP01; RIGHT, UP and DOWN stay in it.
        name, top, lines, cut = key(vm, "select", NOTE)
        assert (name, top, cut) == ("SHOP01.TXT", "0", ""), vm.serial
        golden(settled(vm, tmp_path), "data_note")
        for k in ("right", "up", "down"):
            press(vm, k)
        quiet(vm, NOTES)
        assert tabs(vm) == ["DATA"], vm.serial
        # LEFT goes back with the same note selected.
        assert key(vm, "left", NOTES) == ("2", "0", "4")
        # So does the Back button.
        key(vm, "select", NOTE)
        assert key(vm, "back", NOTES) == ("2", "0", "4")
        # The dial leaves with a note open; the return shows the list.
        key(vm, "select", NOTE)
        script(vm, "dial 1")
        assert wait_tabs(vm, 2) == ["DATA", "MAP"], vm.serial
        before = count(vm, NOTES)
        script(vm, "dial -1")
        assert wait_tabs(vm, 3) == ["DATA", "MAP", "DATA"], vm.serial
        assert wait_count(vm, NOTES, before + 1)[-1] == ("2", "0", "4")
        # LEFT on NOTES goes to INV.
        press(vm, "left")
        assert wait_tabs(vm, 4)[-1] == "INV", vm.serial
        no_exception(vm)


def test_data_no_notes(board, kernel_image, card, tmp_path, need_mtools):
    for name in SAMPLE_NOTES:
        mtools("mdel", "-i", card_target(card), f"::PIPBOY/DATA/NOTES/{name}")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_data(vm) == ("0", "0", "0")
        golden(settled(vm, tmp_path), "data_notes_empty")
        press(vm, "select")
        quiet(vm, NOTE)
        no_exception(vm)


@pytest.fixture(scope="session")
def tapes():
    """The two test holotapes of tests/tapes/, built with the Pip-OS SDK
    into build/tapes/."""
    out = {}
    for name, image in TAPES.items():
        build = ROOT / "build" / "tapes" / name
        subprocess.run(["make", "-C", str(ROOT / "tests" / "tapes" / name),
                        f"PIPOS_SDK={ROOT / 'external' / 'pip-os' / 'sdk'}", f"BUILD={build}",
                        "CI=1"], check=True, env=minimal_env(), capture_output=True)
        out[name] = build / image
    return out


def add_tapes(card, tapes, names):
    target = card_target(card)
    try:
        mtools("mmd", "-i", target, "::PIPBOY/HOLO")
    except subprocess.CalledProcessError:
        pass  # make sdimage made the folder already
    for name in names:
        mtools("mcopy", "-o", "-i", target, str(tapes[name]), f"::PIPBOY/HOLO/{TAPES[name]}")


def to_tapes(vm, dial="dial 2"):
    to_data(vm, dial)
    before = count(vm, TAPES_LINE)
    press(vm, "right")
    return wait_count(vm, TAPES_LINE, before + 1)[-1]


def test_data_no_tapes(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_tapes(vm) == ("0", "0", "0", "")
        golden(settled(vm, tmp_path), "data_tapes_empty")
        press(vm, "select")
        quiet(vm, TAPE_RC)
        no_exception(vm)


def test_data_tapes(board, kernel_image, card, tmp_path, need_mtools, tapes):
    add_tapes(card, tapes, ["linetest", "fault"])
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        # The radio plays first: RADIO is one detent back from STAT.
        script(vm, "dial -1")
        wait_line(vm, "deitrix: tab=RADIO")
        press(vm, "select")
        wait_line(vm, "deitrix: radio station=1")
        assert to_tapes(vm, "dial 3") == ("0", "0", "2", "")
        golden(settled(vm, tmp_path), "data_tapes")

        # LINETEST runs, draws, and ends with status 42 on SELECT.
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, 'linetest: ready args ""')
        # The radio's sound stopped before the tape started.
        start = vm.serial.index("ev=holo_start", mark)
        assert "op=stop" in vm.serial[mark:start], vm.serial[mark:]
        assert "deitrix: data launch LINETEST.HTP" in vm.serial[mark:start], vm.serial
        assert TAPE_RC.findall(vm.serial) == [], vm.serial
        before = count(vm, TAPES_LINE)
        press(vm, "select")
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == \
            ("0", "0", "2", "Tape ended, status 42.")
        assert TAPE_RC.findall(vm.serial)[-1] == "42", vm.serial
        golden(settled(vm, tmp_path), "data_tapes_status")
        # The radio did not start again.
        after = vm.serial[vm.serial.index("deitrix: data tape rc=42"):]
        assert "op=seq" not in after, after

        # FAULT is stopped by the kernel; the ROM goes on and redraws.
        assert key(vm, "down", TAPES_LINE) == ("1", "0", "2", "")
        before = count(vm, TAPES_LINE)
        press(vm, "select")
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == \
            ("1", "0", "2", "Tape was stopped by the system.")
        assert "faulttest: fault now" in vm.serial, vm.serial
        assert "ev=holo_kill" in vm.serial, vm.serial
        assert TAPE_RC.findall(vm.serial)[-1] == "-14", vm.serial
        golden(settled(vm, tmp_path), "data_tapes_fault")
        rom_running(vm)

        # The line goes when the selection moves.
        assert key(vm, "up", TAPES_LINE) == ("0", "0", "2", "")
        # RADIO shows the radio off.
        script(vm, "dial 2")
        wait_line(vm, "deitrix: radio shows Off")
        no_exception(vm)


CLOCK = serial_line(r"deitrix: data clock (.*?) \| (.*?)")
SETTER = serial_line(r"deitrix: data setter field=(\d) (\S+ \S+) \| (.*?)")
RTC_DEVICE = ["-device", "ds1338,bus=i2c-bus.1,address=0x68"]
SET_NOTE = "Time set. A clock chip keeps it."


def to_clock(vm, dial="dial 2"):
    to_tapes(vm, dial)
    before = count(vm, CLOCK)
    press(vm, "right")
    return wait_count(vm, CLOCK, before + 1)[-1]


def test_data_clock_without_rtc(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_clock(vm) == ("none", "")
        golden(settled(vm, tmp_path), "data_clock_none")
        for k in ("up", "down", "back"):
            press(vm, k)
        quiet(vm, SETTER)

        # The setter starts at 2026-01-01 00:00 without a clock.
        assert key(vm, "select", SETTER) == ("0", "2026-01-01 00:00", "")
        # LEFT on the first field cancels.
        assert key(vm, "left", CLOCK) == ("none", "")
        # So does Back, from any field.
        key(vm, "select", SETTER)
        assert key(vm, "right", SETTER) == ("1", "2026-01-01 00:00", "")
        assert key(vm, "up", SETTER) == ("1", "2026-02-01 00:00", "")
        assert key(vm, "back", CLOCK) == ("none", "")
        assert "data set_time" not in vm.serial, vm.serial

        # A known state for the golden: month active, 2026-02-01 07:00.
        key(vm, "select", SETTER)
        for k in ("right", "right", "right"):
            key(vm, k, SETTER)
        for _ in range(7):
            key(vm, "up", SETTER)
        assert key(vm, "left", SETTER) == ("2", "2026-01-01 07:00", "")
        key(vm, "left", SETTER)
        assert key(vm, "up", SETTER) == ("1", "2026-02-01 07:00", "")
        golden(settled(vm, tmp_path), "data_setter")
        # RIGHT on the last field stays in the setter and in the tab.
        for field in ("2", "3", "4"):
            assert key(vm, "right", SETTER)[0] == field
        n = len(tabs(vm))
        press(vm, "right")
        quiet(vm, SETTER)
        assert len(tabs(vm)) == n, vm.serial

        # SELECT stores it: without a chip the clock runs from there.
        before = count(vm, CLOCK)
        press(vm, "select")
        when, note = wait_count(vm, CLOCK, before + 1)[-1]
        assert "deitrix: data set_time 1769929200 rc=0" in vm.serial, vm.serial
        assert re.fullmatch(r"07:00:0\d 2026-02-01 Sunday", when), when
        assert note == SET_NOTE
        # The page ticks each second.
        later = wait_count(vm, CLOCK, count(vm, CLOCK) + 2)[-1]
        assert later[0] > when, (when, later)
        # The header clock now shows the time too.
        no_exception(vm)


def test_data_clock_with_rtc(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card,
                  extra_args=RTC_DEVICE + ["-rtc", "base=2026-11-23T09:30:00"]) as vm:
        wait_ready(vm)
        when, note = to_clock(vm)
        assert re.fullmatch(r"09:30:\d\d 2026-11-23 Monday", when), when
        assert note == ""
        # The setter starts from the clock; one year up, then store.
        assert key(vm, "select", SETTER)[1] in ("2026-11-23 09:30", "2026-11-23 09:31")
        _, start, _ = key(vm, "up", SETTER)
        assert start.startswith("2027-11-23 09:3"), start
        before = count(vm, CLOCK)
        press(vm, "select")
        when, note = wait_count(vm, CLOCK, before + 1)[-1]
        assert note == SET_NOTE
        assert when.startswith(start[11:] + ":0") and when.endswith("2027-11-23 Tuesday"), when
        vm.wait_for(re.compile(r"ev=rtc_set rc=0"), timeout=TIMEOUT)
        # The chip holds the new time.
        reply = script(vm, "rtc")
        line = reply.lines[0]
        assert "rc=0" in line and f"time=2027-11-23T{start[11:]}" in line, reply
        no_exception(vm)
