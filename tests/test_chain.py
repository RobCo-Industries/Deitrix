"""The whole road on one card: boot, the five tabs, a holotape, a save
and a restart.

The test boots only the images of make sdimage. Each other test covers
one part; this one runs the parts in a row on the same card, with
Pip-OS at its head. A nightly run in another repository picks it with
-k whole_chain.
"""

import re
import time

from conftest import serial_line
from pipos_testing import TIMEOUT, no_exception, screenshot, script
from test_culvert import ended, key, launch, quit_from_play
from test_data import to_tapes
from test_deitrix import boot_rom, golden, rom_running, tabs, wait_ready, wait_tabs
from test_model import TAB_LINE, load_lines, select, wait_line

KERNEL_LOAD = serial_line(r"@RC stage=pipos ev=load [^\n]*?")
HOLO_START = serial_line(r"@RC stage=pipos ev=holo_start name=(\S+)[^\n]*?")
DEITRIX_SAVED = serial_line(r"deitrix: save DEITRIX\.SAV ok")


def wait_saved_after(vm, line):
    """Waits for a save logged after `line`, so the file holds that change."""
    deadline = time.monotonic() + TIMEOUT
    while True:
        at = vm.serial.rfind(line)
        if at >= 0 and DEITRIX_SAVED.search(vm.serial, at):
            return
        assert time.monotonic() < deadline, f"no save after {line}\n{vm.serial}"
        time.sleep(0.05)


def test_whole_chain_pipboy(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        # Boot: Pip-OS loads the ROM once and jumps to it.
        assert "@RC stage=pipos ev=boot " in vm.serial, vm.serial
        assert len(KERNEL_LOAD.findall(vm.serial)) == 1, vm.serial
        assert "@RC stage=pipos ev=jump " in vm.serial, vm.serial
        wait_ready(vm)
        # A fresh card: no save yet.
        assert any(re.fullmatch(r"deitrix: load DEITRIX\.SAV missing tab=0 volume=\d+ sound=1 "
                                r"station=0", l) for l in load_lines(vm)), vm.serial
        golden(screenshot(vm, tmp_path), "stat")

        # The five tabs, one detent at a time.
        script(vm, "dial 1")
        assert wait_tabs(vm, 1) == ["INV"], vm.serial
        golden(screenshot(vm, tmp_path), "inv")
        for n in range(2, 6):
            script(vm, "dial 1")
            wait_tabs(vm, n)
        assert tabs(vm) == ["INV", "DATA", "MAP", "RADIO", "STAT"], vm.serial

        # A holotape: Culvert from TAPES, out through the pause menu.
        to_tapes(vm)
        assert wait_tabs(vm, 6)[-1] == "DATA", vm.serial
        launch(vm)
        assert HOLO_START.findall(vm.serial) == ["CULVERT.HTP"], vm.serial
        key(vm, "select", r"level n=1 .*")
        quit_from_play(vm)
        ended(vm)
        assert "@RC stage=pipos ev=holo_exit status=0" in vm.serial, vm.serial
        assert "ev=holo_kill" not in vm.serial, vm.serial
        assert "culvert: solved" not in vm.serial, vm.serial

        # A save: station 1 on RADIO, then the autosave. The launch and
        # the tab change can each write DEITRIX.SAV before the station
        # changes, so wait for a save logged after the station line.
        script(vm, "dial 2")
        assert wait_tabs(vm, 7)[-1] == "RADIO", vm.serial
        select(vm, "deitrix: radio station=1")
        wait_saved_after(vm, "deitrix: radio station=1")
        no_exception(vm)

    # A restart on the same card: it opens on RADIO with station 1.
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert any(re.fullmatch(r"deitrix: load DEITRIX\.SAV ok tab=4 volume=\d+ sound=1 "
                                r"station=1", l) for l in load_lines(vm)), vm.serial
        assert TAB_LINE.findall(vm.serial) == [], vm.serial
        script(vm, "dial 1")
        wait_line(vm, "deitrix: tab=STAT")
        rom_running(vm)
        no_exception(vm)
