"""The hum in QEMU: "UI sounds" on "Full + hum".

QEMU has no sound device, so the test reads the audio trace. A play is
an "op=pcm" line and a queued buffer that takes over is an
"op=pcm_next" line, each with its length and CRC-32. The test names
each buffer by the lengths and CRCs of src/services/sfx_table.h: the
hum's two parts, each table, and each head and rest over the hum. Each
step waits for trace lines, never for a fixed time.
"""

from conftest import serial_line
from pipos_testing import no_exception, press, script
from test_deitrix import boot_rom, wait_ready, wait_tabs
from test_model import wait_line
from test_sfx import TABLES, inv_key, wait_for
from test_weep import WEEP_CRC, sound_on, write_settings

# Every play and every queued buffer, in trace order.
AUDIO = serial_line(r"@RC stage=pipos ev=audio op=(pcm|pcm_next) (?:rate=\d+ )?len=(\d+) "
                    r".*?crc=([0-9a-fA-F]+).*?")
TONE = serial_line(r"@RC stage=pipos ev=audio op=tone .*?")
NAMES = {v: k for k, v in TABLES.items()}
NAMES[(5250, WEEP_CRC)] = "WEEP"
LOADED = "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=3 station=0"


def audio(vm, at=0):
    """[(op, name)] of every play and queued buffer after offset `at`.
    A buffer the header does not name, a disk click, reads "?"."""
    return [(m[1], NAMES.get((int(m[2]), int(m[3], 16)), "?"))
            for m in AUDIO.finditer(vm.serial, at)]


def hum_lines(vm, at=0):
    return [a for a in audio(vm, at) if a[1] in ("HUM_1", "HUM_2")]


def boot_hum(kernel_image, board, card, tmp_path):
    sound_on(card, tmp_path)
    write_settings(card, tmp_path, ui_sound=3)
    return boot_rom(kernel_image, board, sd=card)


def wait_hum_start(vm, at):
    """Waits for part one to start after offset `at`."""
    wait_for(vm, "no hum start", lambda: ("pcm", "HUM_1") in audio(vm, at))


def test_hum_after_the_weep_and_its_parts_alternate(board, kernel_image, card, tmp_path,
                                                   need_mtools):
    with boot_hum(kernel_image, board, card, tmp_path) as vm:
        wait_ready(vm)
        wait_line(vm, LOADED)
        wait_for(vm, "wanted three parts",
                 lambda: len([a for a in hum_lines(vm) if a[0] == "pcm_next"]) >= 2)
        got = audio(vm)
        weep = got.index(("pcm", "WEEP"))
        first = got.index(("pcm", "HUM_1"))
        assert weep < first, got
        hums = hum_lines(vm, 0)
        start = hums.index(("pcm", "HUM_1"))
        assert hums[start:start + 3] == [("pcm", "HUM_1"), ("pcm_next", "HUM_2"),
                                         ("pcm_next", "HUM_1")], got
        no_exception(vm)


def test_a_row_move_plays_the_head_then_the_rest(board, kernel_image, card, tmp_path,
                                                 need_mtools):
    with boot_hum(kernel_image, board, card, tmp_path) as vm:
        wait_ready(vm)
        wait_line(vm, LOADED)
        wait_hum_start(vm, 0)
        mark = len(vm.serial)
        script(vm, "dial 1")
        assert wait_tabs(vm, 1)[-1] == "INV", vm.serial
        wait_for(vm, "no mode head", lambda: ("pcm", "MODE_HEAD") in audio(vm, mark))
        mark = len(vm.serial)
        inv_key(vm, "down", "cat=0 row=1")
        wait_for(vm, "no scroll rest", lambda: ("pcm_next", "SCROLL_REST") in audio(vm, mark))
        got = [a for a in audio(vm, mark) if a[1] != "?"]
        head = got.index(("pcm", "SCROLL_HEAD"))
        assert got[head + 1] == ("pcm_next", "SCROLL_REST"), got
        # The hum's part two follows the rest.
        wait_for(vm, "no part two after the rest",
                 lambda: ("pcm_next", "HUM_2") in audio(vm, mark))
        assert ("pcm", "SCROLL") not in got, got
        no_exception(vm)


def test_a_station_stops_the_hum_and_its_end_brings_it_back(board, kernel_image, card, tmp_path,
                                                             need_mtools):
    with boot_hum(kernel_image, board, card, tmp_path) as vm:
        wait_ready(vm)
        wait_line(vm, LOADED)
        wait_hum_start(vm, 0)
        script(vm, "dial -1")
        assert wait_tabs(vm, 1)[-1] == "RADIO", vm.serial
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "deitrix: radio station=1")
        # The tuner plays plain, then the tune: many tones, more than a
        # second of top-ups.
        wait_for(vm, "no tuner", lambda: ("pcm", "TUNER") in audio(vm, mark))
        wait_for(vm, "wanted the tune", lambda: len(TONE.findall(vm.serial, mark)) >= 12)
        tuner = vm.serial.index("len=%d " % TABLES["TUNER"][0], mark)
        assert hum_lines(vm, tuner) == [], vm.serial[mark:]
        assert ("pcm", "TUNER_HEAD") not in audio(vm, mark), vm.serial[mark:]
        # SELECT stops the station: the tuner, then the hum again.
        stop = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "deitrix: radio off")
        wait_hum_start(vm, stop)
        got = [a for a in audio(vm, stop) if a[1] != "?"]
        assert got.index(("pcm", "TUNER")) < got.index(("pcm", "HUM_1")), got
        no_exception(vm)


def test_a_tape_stops_the_hum_and_its_return_brings_it_back(board, kernel_image, card, tmp_path,
                                                            need_mtools):
    with boot_hum(kernel_image, board, card, tmp_path) as vm:
        wait_ready(vm)
        wait_line(vm, LOADED)
        wait_hum_start(vm, 0)
        script(vm, "dial 2")
        assert wait_tabs(vm, 1)[-1] == "DATA", vm.serial
        mark = len(vm.serial)
        press(vm, "right")
        wait_for(vm, "no tapes line", lambda: "deitrix: data tapes " in vm.serial[mark:])
        mark = len(vm.serial)
        press(vm, "select")
        wait_line(vm, "culvert: title")
        start = vm.serial.index("ev=holo_start", mark)
        # The start sound plays plain and ends before the tape.
        before = audio(vm, mark)
        assert ("pcm", "TAPE_START") in before, vm.serial[mark:]
        assert "op=pcm_end" in vm.serial[vm.serial.index("len=%d " % TABLES["TAPE_START"][0],
                                                         mark):start]
        assert hum_lines(vm, vm.serial.index("len=%d " % TABLES["TAPE_START"][0], mark)) == []
        press(vm, "down")
        wait_line(vm, "culvert: menu item=1")
        press(vm, "select")
        wait_line(vm, "deitrix: data tape rc=0")
        back = vm.serial.index("deitrix: data tape rc=0", mark)
        wait_hum_start(vm, back)
        got = [a for a in audio(vm, back) if a[1] != "?"]
        stop = got.index(("pcm", "TAPE_STOP"))
        assert stop < got.index(("pcm", "HUM_1")), got
        # The stop sound ended before the hum started.
        at = vm.serial.index("len=%d " % TABLES["TAPE_STOP"][0], back)
        hum = vm.serial.index("len=%d " % TABLES["HUM_1"][0], at)
        assert "op=pcm_end" in vm.serial[at:hum], vm.serial[back:]
        no_exception(vm)
