"""The RADIO tab in QEMU: its views, the radio it drives, its settings.

The tab logs one "deitrix: radio ..." line for each key it takes, before
the redraw. A key that changes nothing logs "deitrix: radio idle". So a
test that needs a frame on screen sends Back, which the tab never uses,
and waits for that line: the shell draws before it takes the next event.

QEMU has no sound device: Pip-OS prints each audio call as a trace line
"@RC stage=pipos ev=audio op=...". The trace is flushed late, so a test
looks for the ops it wants after a mark and waits for them.
"""

import re
import time

import pytest

from test_deitrix import boot_rom, golden, tabs, wait_ready, wait_tabs
from test_model import audio_after, card_target, mtools, wait_line
from pipos_testing import TIMEOUT, no_exception, press, screenshot, script

# Whole lines only: the serial buffer can end in the middle of one. The
# lines a frame or the service logs are not key lines.
KEY_LINE = re.compile(r"^deitrix: radio (?!shows |underrun|cannot play)(.*?)\r?\n", re.M)
SAVE_OK = re.compile(r"^deitrix: save DEITRIX\.SAV ok\r?\n", re.M)
UNDERRUN = "deitrix: radio underrun"
# The UI tones of src/ui/core.c: MOVE, SELECT, BACK, DENY.
UI_HZ = {1400, 1900, 900, 180}

# The sample card: stations 1 to 3 are tunes, 4 and 5 PCM tracks. Rows 0
# to 4 are the stations, 5 the volume, 6 the UI sounds.
ROW_VOLUME = 5
ROW_SOUND = 6


def key_lines(vm):
    return KEY_LINE.findall(vm.serial)


def key(vm, name, want):
    """Taps a key and waits for the tab's next line, which must be `want`."""
    before = len(key_lines(vm))
    press(vm, name)
    deadline = time.monotonic() + TIMEOUT
    while len(key_lines(vm)) <= before:
        assert time.monotonic() < deadline, f"no radio line after {name}\n{vm.serial}"
        time.sleep(0.05)
    got = key_lines(vm)[before]
    assert got == want, f"{name}: {got!r} != {want!r}\n{vm.serial}"


def shot(vm, tmp_path, name):
    """Makes sure the last frame is on screen, then compares it."""
    key(vm, "back", "idle")
    golden(screenshot(vm, tmp_path), name)


def to_radio(vm, count=1):
    script(vm, "dial -1")
    assert wait_tabs(vm, count)[-1] == "RADIO", vm.serial


def walk(vm, start, end):
    """Moves the cursor from row `start` to row `end`."""
    step = "down" if end > start else "up"
    d = 1 if end > start else -1
    for row in range(start + d, end + d, d):
        key(vm, step, f"row={row}")


def wait_ops(vm, mark, op, count=1, timeout=TIMEOUT):
    """Waits for `count` audio ops `op` after the line `mark`, returns all ops."""
    deadline = time.monotonic() + timeout
    while True:
        ops = [o for o, _ in audio_after(vm, mark)]
        if ops.count(op) >= count:
            return ops
        assert time.monotonic() < deadline, f"wanted {count} op={op}\n{vm.serial}"
        time.sleep(0.1)


def start_level(vm):
    m = re.search(r"^deitrix: load DEITRIX\.SAV \w+.*? volume=(\d+) ", vm.serial, re.M)
    assert m, vm.serial
    return int(m[1])


def test_radio_views(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        shot(vm, tmp_path, "radio")
        # UP at the top, and LEFT on a station row changes the tab.
        key(vm, "up", "end row=0")
        press(vm, "left")
        assert wait_tabs(vm, 2)[-1] == "MAP", vm.serial
        press(vm, "right")
        assert wait_tabs(vm, 3)[-1] == "RADIO", vm.serial
        # A tune: its first note is G4, 392 Hz.
        key(vm, "select", "station=1")
        wait_line(vm, "deitrix: radio shows Playing Morning")
        shot(vm, tmp_path, "radio_playing")
        tones = [rest for op, rest in audio_after(vm, "deitrix: radio station=1")
                 if op == "tone"]
        assert any(" hz=392 " in t for t in tones), vm.serial
        # Switch to a PCM station: the tune stops, the stream starts.
        walk(vm, 0, 3)
        key(vm, "select", "station=4")
        wait_line(vm, "deitrix: radio shows Playing Hearth")
        ops = wait_ops(vm, "deitrix: radio station=4", "pcm_next")
        assert ops.count("pcm") == 1, ops
        # The ROM stops the tune first, but the trace shows no op=stop
        # between the tune and op=pcm, so only the stream is checked.
        assert "pcm_end" not in ops, ops
        # SELECT on the playing station stops it.
        key(vm, "select", "off")
        wait_line(vm, "deitrix: radio shows Off")
        wait_ops(vm, "deitrix: radio off", "stop")
        # The settings rows: LEFT and RIGHT stay on the tab.
        walk(vm, 3, ROW_VOLUME)
        shot(vm, tmp_path, "radio_volume")
        level = start_level(vm)
        down = max(0, ((level + 9) // 10 - 1) * 10)
        key(vm, "left", f"volume={down}")
        key(vm, "right", f"volume={min(100, down + 10)}")
        key(vm, "down", f"row={ROW_SOUND}")
        key(vm, "down", f"end row={ROW_SOUND}")
        key(vm, "right", "sound=0")
        key(vm, "left", "sound=1")
        key(vm, "select", "sound=0")
        key(vm, "select", "sound=1")
        assert tabs(vm) == ["RADIO", "MAP", "RADIO"], vm.serial
        no_exception(vm)


def test_radio_volume_limits(board, kernel_image, card, need_mtools):
    """RIGHT to 100 and LEFT to 0: one line per step, then refused."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        walk(vm, 0, ROW_VOLUME)
        v = start_level(vm)
        while v < 100:
            v = min(100, (v // 10 + 1) * 10)
            key(vm, "right", f"volume={v}")
        key(vm, "right", "refused volume=100")
        while v > 0:
            v = max(0, ((v + 9) // 10 - 1) * 10)
            key(vm, "left", f"volume={v}")
        key(vm, "left", "refused volume=0")
        assert tabs(vm) == ["RADIO"], vm.serial
        no_exception(vm)


def test_radio_survives_a_reboot(board, kernel_image, card, tmp_path, need_mtools):
    """A station, the volume and UI sounds reach DEITRIX.SAV. The next boot
    opens on RADIO with the station picked, not playing."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        walk(vm, 0, 3)
        key(vm, "select", "station=4")
        key(vm, "select", "off")
        walk(vm, 3, ROW_VOLUME)
        level = start_level(vm)
        want = max(0, ((level + 9) // 10 - 1) * 10)
        key(vm, "left", f"volume={want}")
        key(vm, "down", f"row={ROW_SOUND}")
        key(vm, "select", "sound=0")
        # The save after the last change.
        at = vm.serial.index("deitrix: radio sound=0")
        deadline = time.monotonic() + 20
        while not SAVE_OK.search(vm.serial, at):
            assert time.monotonic() < deadline, vm.serial[at:]
            time.sleep(0.1)
        no_exception(vm)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, f"deitrix: load DEITRIX.SAV ok tab=4 volume={want} sound=0 station=4")
        assert tabs(vm) == [], vm.serial
        key(vm, "back", "idle")
        golden(screenshot(vm, tmp_path), "radio_restored")
        # The radio does not resume at start.
        assert "op=pcm" not in vm.serial, vm.serial
        key(vm, "select", "station=4")
        no_exception(vm)


def test_radio_plays_across_the_tabs(board, kernel_image, card, need_mtools):
    """Station 5 keeps streaming through a walk over every tab."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        walk(vm, 0, 4)
        key(vm, "select", "station=5")
        mark = "deitrix: radio station=5"
        ops = wait_ops(vm, mark, "pcm_next")
        start = ops.index("pcm")
        for i, name in enumerate(("STAT", "INV", "DATA", "MAP", "RADIO"), start=2):
            script(vm, "dial 1")
            assert wait_tabs(vm, i)[-1] == name, vm.serial
        ops = wait_ops(vm, mark, "pcm_next", ops.count("pcm_next") + 3)
        assert ops[start:].count("pcm") == 1, ops
        assert "stop" not in ops[start:] and "pcm_end" not in ops[start:], ops
        assert "tone" not in ops[start:], ops
        assert UNDERRUN not in vm.serial
        # Back on RADIO the cursor and the mark are where they were.
        key(vm, "select", "off")
        no_exception(vm)


def test_radio_no_file(board, kernel_image, card, tmp_path, need_mtools):
    mtools("mdel", "-i", card_target(card), "::PIPBOY/DATA/RADIO.INI")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        shot(vm, tmp_path, "radio_empty")
        key(vm, "select", "idle")
        key(vm, "up", "end row=0")
        key(vm, "down", "row=1")
        key(vm, "select", "sound=0")
        assert "deitrix: radio station" not in vm.serial
        no_exception(vm)


def test_radio_station_missing(board, kernel_image, card, tmp_path, need_mtools):
    """A station whose tune is gone: listed dim, SELECT says so, no sound."""
    mtools("mdel", "-i", card_target(card), "::PIPBOY/DATA/RADIO/LULLABY.TUN")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        walk(vm, 0, 2)
        key(vm, "select", "station=3")
        wait_line(vm, "deitrix: radio cannot play")
        wait_line(vm, "deitrix: radio shows Cannot play Lullaby")
        shot(vm, tmp_path, "radio_nothing")
        # Only UI tones follow: the radio stays off, so they play.
        hz = [int(re.search(r" hz=(\d+)", rest)[1])
              for op, rest in audio_after(vm, "deitrix: radio station=3") if op == "tone"]
        assert set(hz) <= UI_HZ, hz
        no_exception(vm)


LONG_S = 60
LONG_BOARDS = ("raspi0", "raspi2b")


def test_radio_long_run(board, kernel_image, card, need_mtools):
    """Station 4 left playing for 60 s: a chunk a second, no end, no gap.

    An underrun shows twice: the service logs "deitrix: radio underrun"
    when it finds the queue dry, and Pip-OS traces op=pcm_end when the
    sound runs out. The track loops, so neither may come."""
    if board not in LONG_BOARDS:
        pytest.skip("the long run is for raspi0 and raspi2b")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        walk(vm, 0, 3)
        key(vm, "select", "station=4")
        mark = "deitrix: radio station=4"
        wait_ops(vm, mark, "pcm")
        began = time.monotonic()
        time.sleep(LONG_S)
        ops = [o for o, _ in audio_after(vm, mark)]
        start = ops.index("pcm")
        played = time.monotonic() - began
        # One chunk a second; the trace may lag a little.
        assert ops[start:].count("pcm_next") >= LONG_S - 5, (played, ops.count("pcm_next"))
        assert "pcm_end" not in ops[start:] and "stop" not in ops[start:], ops
        assert UNDERRUN not in vm.serial
        no_exception(vm)
