"""Saves the serial log of one session with the table sounds, for `make sfx-preview`.

It runs only with --sfx-preview-log PATH. The card is the one make
sdimage writes, its PIPOS.CFG set as for make boot-sound, with the
system sound on, and a DEITRIX.SAV with "UI sounds" on Full. The session:

- the boot, up to the weep and the quiet after it
- the dial through the other four modes, STAT to RADIO
- a station on, then off
- five rows down on RADIO
- the dial to INV, RIGHT to the next category, SELECT, BACK
- the dial to DATA, RIGHT to TAPES, a tape launched and left by Quit

The kernel keeps only the newest sound request inside one 10 ms tick,
so each input waits for its sound before the next one. Each part ends
with about half a second of quiet. RobCo-Core's tools/audio_preview.py
turns the log into a WAV file.
"""

import re
import time

import pytest

from pipos_testing import TIMEOUT, no_exception, press, script
from test_boot_sound_log import CFG, CFG_PATH, wait_quiet
from test_deitrix import boot_rom, wait_ready, wait_tabs
from test_model import card_target, mtools, wait_line
from test_sfx import MODE, SCROLL, SELECT, TAB, HIGHLIGHT, plays_after
from test_weep import WEEP, write_settings

GAP_S = 0.5
STATION_S = 3.0
SOUND = re.compile(r"ev=audio op=(?:pcm|tone) ")


def wait_for(vm, what, done):
    deadline = time.monotonic() + TIMEOUT
    while not done():
        assert time.monotonic() < deadline, f"{what}\n{vm.serial}"
        time.sleep(0.05)


def sounded(vm, what, act):
    """Runs `act`, waits for a sound to start after it, then the gap."""
    mark = len(vm.serial)
    act()
    wait_for(vm, f"no sound after {what}", lambda: SOUND.search(vm.serial, mark))
    time.sleep(GAP_S)


def dial(vm, n, tab, count):
    sounded(vm, f"dial {n}", lambda: script(vm, f"dial {n}"))
    assert wait_tabs(vm, count)[-1] == tab, vm.serial


def keyed(vm, name, line):
    """Taps a key, waits for the ROM's line and for a sound."""
    def act():
        before = vm.serial.count(line)
        press(vm, name)
        wait_for(vm, f"no {line!r} after {name}", lambda: vm.serial.count(line) > before)
    sounded(vm, name, act)


def test_save_session_log(request, board, kernel_image, card, tmp_path, need_mtools):
    path = request.config.getoption("--sfx-preview-log")
    cfg = tmp_path / "PIPOS.CFG"
    cfg.write_text(CFG)
    mtools("mcopy", "-o", "-i", card_target(card), str(cfg), CFG_PATH)
    write_settings(card, tmp_path, ui_sound=2)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
        vm.wait_for(WEEP, timeout=TIMEOUT)
        wait_quiet(vm)
        time.sleep(GAP_S)
        start = len(vm.serial)
        # The other four modes.
        for i, tab in enumerate(("INV", "DATA", "MAP", "RADIO"), 1):
            dial(vm, 1, tab, i)
        time.sleep(GAP_S)
        # A station on and off. The radio blocks the UI sounds while it plays.
        keyed(vm, "select", "deitrix: radio station=1")
        time.sleep(STATION_S)
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "deitrix: radio off")
        wait_for(vm, "no stop after the station", lambda: "op=stop" in vm.serial[mark:])
        time.sleep(GAP_S * 2)
        # Five rows down.
        for row in range(1, 6):
            keyed(vm, "down", f"deitrix: radio row={row}")
        time.sleep(GAP_S)
        # A sub-tab, SELECT and BACK on INV.
        dial(vm, 2, "INV", 5)
        keyed(vm, "right", "deitrix: inv cat=1")
        keyed(vm, "select", "deitrix: inv open=")
        keyed(vm, "back", "deitrix: inv back")
        time.sleep(GAP_S)
        # A tape launched and left: Quit on its title.
        dial(vm, 1, "DATA", 6)
        keyed(vm, "right", "deitrix: data tapes ")
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "culvert: title")
        assert "ev=holo_start" in vm.serial[mark:], vm.serial[mark:]
        time.sleep(GAP_S * 2)
        press(vm, "down")
        wait_line(vm, "culvert: menu item=1")
        time.sleep(GAP_S)
        press(vm, "select")
        wait_line(vm, "deitrix: data tape rc=0")
        wait_quiet(vm)
        time.sleep(GAP_S)
        no_exception(vm)
        log = vm.serial
        got = plays_after(vm, start)
    with open(path, "w", encoding="utf-8") as f:
        f.write(log)
    for name, table in (("mode", MODE), ("tab", TAB), ("scroll", SCROLL),
                        ("select", SELECT), ("highlight", HIGHLIGHT)):
        if table not in got:
            pytest.fail("the session played no %s\n%s" % (name, log[start:]))
