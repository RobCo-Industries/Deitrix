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

ROOT = pathlib.Path(__file__).resolve().parent.parent
TAPES = {"linetest": "LINETEST.HTP", "fault": "FAULT.HTP"}

NOTES = re.compile(r"^deitrix: data notes sel=(\d+) top=(\d+) count=(\d+)\r?$", re.M)
NOTE = re.compile(r"^deitrix: data note (\S+) top=(\d+) lines=(\d+)( cut)?\r?$", re.M)
TAPES_LINE = re.compile(r"^deitrix: data tapes sel=(\d+) top=(\d+) count=(-?\d+) \| (.*?)\r?$",
                        re.M)
TAPE_RC = re.compile(r"^deitrix: data tape rc=(-?\d+)\r?$", re.M)
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
