"""The tape and tuner sounds in QEMU: "UI sounds" on Full.

The test reads the audio trace and finds each table by the length and
CRC-32 of src/services/sfx_table.h. The trace is flushed late, so each
step waits for its sound's line before the next input.
"""

import time

from conftest import serial_line
from pipos_testing import TIMEOUT, no_exception, press, script
from test_deitrix import boot_rom, wait_ready, wait_tabs
from test_model import wait_line
from test_sfx import SELECT, TABLES, plays_after, wait_for
from test_weep import SETTLE_S, sound_on, write_settings

TUNER = (37500,) + TABLES["TUNER"]
TAPE_START = (37500,) + TABLES["TAPE_START"]
TAPE_STOP = (37500,) + TABLES["TAPE_STOP"]
# One table play with its t_ms.
PLAY = serial_line(r"@RC stage=pipos ev=audio op=pcm rate=(\d+) len=(\d+) .*?crc=([0-9a-fA-F]+) "
                   r"t_ms=(\d+).*?")
TONE_T = serial_line(r"@RC stage=pipos ev=audio op=tone hz=[1-9]\d* .*?t_ms=(\d+).*?")
PCM_END = "ev=audio op=pcm_end"
# The kernel's timer tick: the trace's own jitter.
TICK_MS = 10


def timed_plays(vm, at, table):
    """The t_ms of each play of `table` after serial offset `at`."""
    return [int(m[4]) for m in PLAY.finditer(vm.serial, at)
            if (int(m[1]), int(m[2]), int(m[3], 16)) == table]


def boot_full(kernel_image, board, card, tmp_path):
    sound_on(card, tmp_path)
    write_settings(card, tmp_path, ui_sound=2)
    vm = boot_rom(kernel_image, board, sd=card)
    return vm


def test_tape_start_ends_before_the_launch(board, kernel_image, card, tmp_path, need_mtools):
    with boot_full(kernel_image, board, card, tmp_path) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
        time.sleep(SETTLE_S)
        script(vm, "dial 2")
        assert wait_tabs(vm, 1)[-1] == "DATA", vm.serial
        mark = len(vm.serial)
        press(vm, "right")
        wait_for(vm, "no tapes line", lambda: "deitrix: data tapes " in vm.serial[mark:])
        time.sleep(SETTLE_S)
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "culvert: title")
        start = vm.serial.index("ev=holo_start", mark)
        before = vm.serial[mark:start]
        # The start sound played and ended before the kernel started the tape.
        assert timed_plays(vm, mark, TAPE_START) != [], vm.serial[mark:]
        assert vm.serial.index("len=%d " % TAPE_START[1], mark) < start, vm.serial[mark:]
        assert PCM_END in before[before.index("len=%d " % TAPE_START[1]):], before
        assert SELECT not in plays_after(vm, mark), vm.serial[mark:]
        # Quit the tape: the stop sound follows its return.
        press(vm, "down")
        wait_line(vm, "culvert: menu item=1")
        press(vm, "select")
        wait_line(vm, "deitrix: data tape rc=0")
        back = vm.serial.index("deitrix: data tape rc=0", mark)
        wait_for(vm, "no tape stop sound", lambda: timed_plays(vm, back, TAPE_STOP) != [])
        time.sleep(SETTLE_S)
        assert [p for p in plays_after(vm, back) if p in (TAPE_STOP, SELECT)] == [TAPE_STOP], \
            vm.serial[back:]
        no_exception(vm)


def test_tuner_before_a_station_and_after_its_stop(board, kernel_image, card, tmp_path,
                                                  need_mtools):
    with boot_full(kernel_image, board, card, tmp_path) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
        time.sleep(SETTLE_S)
        script(vm, "dial -1")
        assert wait_tabs(vm, 1)[-1] == "RADIO", vm.serial
        time.sleep(SETTLE_S)
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "deitrix: radio station=1")
        wait_for(vm, "no tuner", lambda: timed_plays(vm, mark, TUNER) != [])
        tuner = timed_plays(vm, mark, TUNER)[0]
        wait_for(vm, "no station sound",
                 lambda: [int(m[1]) for m in TONE_T.finditer(vm.serial, mark)] != [])
        first = [int(m[1]) for m in TONE_T.finditer(vm.serial, mark)][0]
        assert first >= tuner + 100 - TICK_MS, (tuner, first, vm.serial[mark:])
        assert SELECT not in plays_after(vm, mark), vm.serial[mark:]
        time.sleep(SETTLE_S)
        # SELECT again stops the station, and the tuner plays again.
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "deitrix: radio off")
        wait_for(vm, "no tuner after the stop", lambda: timed_plays(vm, mark, TUNER) != [])
        time.sleep(SETTLE_S)
        assert [p for p in plays_after(vm, mark) if p in (TUNER, SELECT)] == [TUNER], \
            vm.serial[mark:]
        no_exception(vm)
