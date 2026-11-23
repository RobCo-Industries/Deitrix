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

import re
import time

from test_deitrix import boot_rom, golden, tabs, wait_ready, wait_tabs
from test_model import card_target, mtools
from test_stat import count, key, quiet, settled, wait_count
from pipos_testing import no_exception, press, script

NOTES = re.compile(r"^deitrix: data notes sel=(\d+) top=(\d+) count=(\d+)\r?$", re.M)
NOTE = re.compile(r"^deitrix: data note (\S+) top=(\d+) lines=(\d+)( cut)?\r?$", re.M)
SAMPLE_NOTES = ["GARDEN01.TXT", "RECIPE01.TXT", "SHOP01.TXT", "WALK01.TXT"]


def to_data(vm):
    """From STAT to DATA with the dial: two detents."""
    before = count(vm, NOTES)
    script(vm, "dial 2")
    assert wait_tabs(vm, 1) == ["DATA"], vm.serial
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
