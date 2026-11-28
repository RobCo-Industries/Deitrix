"""The levels view of RADIO in QEMU: the four levels, their gains, the store.

SELECT on the Volume row opens the view. Each key it takes logs one
"deitrix: radio level..." line before the redraw, so key() of
test_radio_tab.py reads them. In the view BACK closes it, so a test
that needs a frame on screen sends a key that logs a line and changes
nothing: UP on the first row, or DOWN on the last one.

Every test but the last one boots a card whose PIPOS.CFG sets the
master and the four levels, so no test depends on a default. Pip-OS
computes one gain per play:

    gain = (master * level * level * trim + 5000) / 10000

with a trim of 100 for the ROM. A start line shows vol= as gain / 100
rounded down, and ends with gain=<n> only when the gain is not a
multiple of 100.
"""

import re
import subprocess
import time

from pipos_testing import minimal_env, no_exception, screenshot, script
from test_deitrix import boot_rom, golden, tabs, wait_ready, wait_tabs
from test_model import card_target
from test_radio_tab import UI_HZ, key, to_radio, walk
from test_sound_kinds import LEVELS, TONE, set_levels, tones_after, wait_for

# The sample card: stations 1 to 3 are tunes, 4 and 5 PCM tracks. Rows 0
# to 4 are the stations, 5 the volume.
ROW_VOLUME = 5
MOVE_HZ = 1400
DENY_HZ = 180
GAIN = re.compile(r"^@RC stage=pipos ev=audio op=gain gain=(\d+) t_ms=\d+\r?$", re.M)
AUDIO = re.compile(r"^@RC stage=pipos ev=audio op=(\w+)", re.M)
SET_PATH = "::PIPBOY/PIPOS.SET"
SET_OK = re.compile(r"^deitrix: save PIPOS\.SET ok\r?$", re.M)
SAVE_OK = re.compile(r"^deitrix: save DEITRIX\.SAV ok\r?$", re.M)
# The time a test waits for a line that must not come.
QUIET_S = 1.5


def gain(master, level):
    """The gain of a ROM's play, by the spec's formula."""
    return (master * level * level * 100 + 5000) // 10000


def fields(master, level):
    """The vol= value and the gain= tail of a start line at these levels."""
    g = gain(master, level)
    return str(g // 100), "" if g % 100 == 0 else f" gain={g}"


def open_line(levels):
    return (f"levels radio={levels['radio']} sfx={levels['sfx']} "
            f"recordings={levels['recordings']} games={levels['games']}")


def open_view(vm, start, levels):
    """From row `start` to the Volume row, then SELECT."""
    walk(vm, start, ROW_VOLUME)
    key(vm, "select", open_line(levels))


def level_walk(vm, start, end):
    """Moves the view's cursor from row `start` to row `end`."""
    step = "down" if end > start else "up"
    d = 1 if end > start else -1
    for row in range(start + d, end + d, d):
        key(vm, step, f"level row={row}")


def shot(vm, tmp_path, name, up):
    """A key that logs a line and changes nothing, then the screen."""
    if up:
        key(vm, "up", "level end row=0")
    else:
        key(vm, "down", "level end row=3")
    golden(screenshot(vm, tmp_path), name)


def tone_of(vm, at, hz):
    """Waits for the first op=tone of `hz` after offset `at`."""
    return wait_for(lambda: [t for t in tones_after(vm, at) if t[0] == hz],
                    f"no tone of {hz} Hz", vm)[0]


def test_levels_view_and_keys(board, kernel_image, card, tmp_path, need_mtools):
    set_levels(card, tmp_path, LEVELS)
    master = LEVELS["volume"]
    assert fields(master, 45) == ("16", " gain=1620")
    assert fields(master, 35) == ("9", " gain=980")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        open_view(vm, 0, LEVELS)
        shot(vm, tmp_path, "radio_levels", up=True)
        # A step on Radio tunes: its tone carries the new radio gain.
        key(vm, "right", "level radio=45")
        at = vm.serial.rindex("deitrix: radio level radio=45")
        tone = tone_of(vm, at, MOVE_HZ)
        assert (tone[1], tone[3]) == fields(master, 45), tone
        # Walk to Games. DOWN on the last row plays the deny tone: once it
        # is in the trace, the next move tone is the step's.
        level_walk(vm, 0, 3)
        mark = len(vm.serial)
        key(vm, "down", "level end row=3")
        deny = vm.serial.index("deitrix: radio level end row=3", mark)
        tone_of(vm, deny, DENY_HZ)
        after = max(m.end() for m in TONE.finditer(vm.serial) if m[1] == str(DENY_HZ))
        key(vm, "right", "level games=35")
        tone = tone_of(vm, after, MOVE_HZ)
        assert (tone[1], tone[3]) == fields(master, 35), tone
        shot(vm, tmp_path, "radio_levels_games", up=False)
        # No level change while nothing plays prints op=gain.
        assert " op=gain " not in vm.serial[at:], vm.serial[at:]
        # SELECT closes the view: LEFT then changes the master.
        key(vm, "select", "levels closed")
        key(vm, "left", "volume=70")
        # Open again and turn the dial: the tab changes and the view closes.
        levels = dict(LEVELS, radio=45, games=35)
        key(vm, "select", open_line(levels))
        script(vm, "dial 1")
        assert wait_tabs(vm, 2)[-1] == "STAT", vm.serial
        script(vm, "dial -1")
        assert wait_tabs(vm, 3)[-1] == "RADIO", vm.serial
        key(vm, "up", "row=4")
        assert tabs(vm) == ["RADIO", "STAT", "RADIO"], vm.serial
        no_exception(vm)


def test_station_follows_its_level(board, kernel_image, card, tmp_path, need_mtools):
    set_levels(card, tmp_path, LEVELS)
    master = LEVELS["volume"]
    assert gain(master, 55) == 2420
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        # Station 4 is a recorded track: it plays as recordings.
        walk(vm, 0, 3)
        key(vm, "select", "station=4")
        start = vm.serial.rindex("deitrix: radio station=4")
        wait_for(lambda: "op=pcm_next" in vm.serial[start:], "no op=pcm_next", vm)
        open_view(vm, 3, LEVELS)
        level_walk(vm, 0, 2)
        key(vm, "right", "level recordings=55")
        at = vm.serial.rindex("deitrix: radio level recordings=55")
        got = wait_for(lambda: GAIN.findall(vm.serial, at), "no op=gain", vm)
        level_walk(vm, 2, 0)
        key(vm, "right", "level radio=45")
        mid = vm.serial.rindex("deitrix: radio level radio=45")
        time.sleep(QUIET_S)
        assert GAIN.findall(vm.serial, at) == [str(gain(master, 55))], vm.serial[at:]
        assert got == [str(gain(master, 55))], got
        assert not GAIN.findall(vm.serial, mid), vm.serial[mid:]
        # The stream goes on: one start, no tone, no stop, no end.
        ops = [m[1] for m in AUDIO.finditer(vm.serial, start)]
        first = ops.index("pcm")
        assert "tone" not in ops[first:], ops
        assert "stop" not in ops[first:] and "pcm_end" not in ops[first:], ops
        assert ops[first:].count("pcm") == 1, ops
        no_exception(vm)


def test_levels_survive_a_reboot(board, kernel_image, card, tmp_path, need_mtools):
    """Two steps reach PIPOS.SET. The next boot starts at them, though
    PIPOS.CFG still says otherwise."""
    set_levels(card, tmp_path, LEVELS)
    master = LEVELS["volume"]
    stored = dict(LEVELS, radio=45, recordings=55)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        open_view(vm, 0, LEVELS)
        key(vm, "right", "level radio=45")
        level_walk(vm, 0, 2)
        key(vm, "right", "level recordings=55")
        key(vm, "select", "levels closed")
        at = vm.serial.rindex("deitrix: radio levels closed")
        deadline = time.monotonic() + 20
        while not (SET_OK.search(vm.serial, at) and SAVE_OK.search(vm.serial)):
            assert time.monotonic() < deadline, vm.serial[at:]
            time.sleep(0.1)
        no_exception(vm)
    text = subprocess.run(["mtype", "-i", card_target(card), SET_PATH], check=True,
                          env=minimal_env(MTOOLS_SKIP_CHECK="1"),
                          capture_output=True).stdout
    assert text == (b"[audio]\nvolume = 80\nradio = 45\nsfx = 100\n"
                    b"recordings = 55\ngames = 30\n"), text
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert " ev=levels " not in vm.serial, vm.serial
        assert tabs(vm) == [], vm.serial
        open_view(vm, 0, stored)
        key(vm, "select", "levels closed")
        walk(vm, ROW_VOLUME, 0)
        key(vm, "select", "station=1")
        mark = vm.serial.rindex("deitrix: radio station=1")
        tune = wait_for(lambda: [t for t in tones_after(vm, mark) if t[0] not in UI_HZ],
                        "no tune", vm)
        assert all((v, tail) == fields(master, 45) for _, v, _, tail in tune), tune
        assert "deitrix: save PIPOS.SET" not in vm.serial, vm.serial
        no_exception(vm)


def test_nothing_stores_by_itself(board, kernel_image, card, need_mtools):
    """The stock card: no level changes, so nothing is stored."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        time.sleep(3)
        assert "deitrix: save PIPOS.SET" not in vm.serial, vm.serial
        no_exception(vm)

