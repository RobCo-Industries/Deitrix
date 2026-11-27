"""The table sounds in QEMU: "UI sounds" on Full.

QEMU has no sound device, so the test reads the audio trace. A table
plays as one "op=pcm rate=37500" line with the length and CRC-32 that
src/services/sfx_table.h gives. Full needs ABI 1.5 and an output that
rests at its midpoint: Pip-OS sets PIP_HW_AUDIO_MID only with [audio]
system_sound on. Without it the RADIO row offers only Off and On, and a
saved Full plays the tones.
"""

import re
import time

from conftest import ROOT, serial_line
from pipos_testing import TIMEOUT, no_exception, press, script
from test_deitrix import boot_rom, wait_ready, wait_tabs
from test_model import wait_line
from test_radio_tab import ROW_SOUND
from test_weep import PCM, SETTLE_S, sound_on, write_settings

TONE = serial_line(r"@RC stage=pipos ev=audio op=tone hz=(\d+).*?")
INV = serial_line(r"deitrix: inv (.*?)")
RADIO_KEY = re.compile(r"^deitrix: radio (?!shows |underrun|cannot play)(.*?)\r?\n", re.M)


def header():
    """{NAME: (len, crc)} of every table in the generated header."""
    text = open(ROOT + "/src/services/sfx_table.h", encoding="ascii").read()
    lens = dict(re.findall(r"#define DX_SFX_(\w+)_LEN (\d+)u", text))
    crcs = dict(re.findall(r"#define DX_SFX_(\w+)_CRC32 0x([0-9a-f]+)u", text))
    return {k: (int(lens[k]), int(crcs[k], 16)) for k in lens}


TABLES = header()
SCROLL = (37500,) + TABLES["SCROLL"]
SELECT = (37500,) + TABLES["SELECT"]
MODE = (37500,) + TABLES["MODE"]
TAB = (37500,) + TABLES["TAB"]
HIGHLIGHT = (37500,) + TABLES["HIGHLIGHT"]


def plays_after(vm, at):
    """The plays of a table after serial offset `at`, found by length and
    CRC. The kernel's disk clicks and the weep play too, and the trace
    is flushed late, so other plays can land after a mark."""
    known = {(37500,) + t for t in TABLES.values()}
    return [p for p in ((int(m[1]), int(m[2]), int(m[3], 16)) for m in PCM.finditer(vm.serial, at))
            if p in known]


def tones_after(vm, at):
    return [int(m[1]) for m in TONE.finditer(vm.serial, at)]


def wait_for(vm, what, done):
    deadline = time.monotonic() + TIMEOUT
    while not done():
        assert time.monotonic() < deadline, f"{what}\n{vm.serial}"
        time.sleep(0.1)


def inv_key(vm, name, prefix):
    """Taps a key on INV and waits for its line, which starts with `prefix`."""
    before = len(INV.findall(vm.serial))
    press(vm, name)
    wait_for(vm, f"no inv line after {name}", lambda: len(INV.findall(vm.serial)) > before)
    got = INV.findall(vm.serial)[before]
    assert got.startswith(prefix), f"{name}: {got!r}\n{vm.serial}"


def radio_key(vm, name):
    """Taps a key on RADIO and returns the tab's next line."""
    before = len(RADIO_KEY.findall(vm.serial))
    press(vm, name)
    wait_for(vm, f"no radio line after {name}", lambda: len(RADIO_KEY.findall(vm.serial)) > before)
    return RADIO_KEY.findall(vm.serial)[before]


def sounds_after(vm, at):
    return len(tones_after(vm, at)) + len(plays_after(vm, at))


def move_and_select(vm):
    """STAT to INV, one row down, SELECT. Returns the serial mark before.

    Each step waits for its sound before the next input. The kernel
    starts a sound on its next tick, and a request made before that tick
    replaces the one waiting, so two quick inputs give one sound."""
    mark = len(vm.serial)
    script(vm, "dial 1")
    assert wait_tabs(vm, 1)[-1] == "INV", vm.serial
    wait_for(vm, "no sound after dial", lambda: sounds_after(vm, mark) >= 1)
    inv_key(vm, "down", "cat=0 row=1")
    wait_for(vm, "no sound after down", lambda: sounds_after(vm, mark) >= 2)
    inv_key(vm, "select", "open=")
    return mark


def test_full_plays_scroll_and_select(board, kernel_image, card, tmp_path, need_mtools):
    sound_on(card, tmp_path)
    write_settings(card, tmp_path, ui_sound=2)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
        at = move_and_select(vm)
        wait_for(vm, "wanted three table plays", lambda: len(plays_after(vm, at)) >= 3)
        time.sleep(SETTLE_S)
        # The dial changes the tab: mode, then the row move and the select.
        assert plays_after(vm, at) == [MODE, SCROLL, SELECT], vm.serial[at:]
        assert tones_after(vm, at) == [], vm.serial[at:]
        no_exception(vm)


def step(vm, key, prefix, want):
    """Taps a key on INV and checks the one table it plays, or none."""
    at = len(vm.serial)
    inv_key(vm, key, prefix)
    if want is None:
        wait_for(vm, f"wanted a tone after {key}", lambda: len(tones_after(vm, at)) >= 1)
    else:
        wait_for(vm, f"wanted a play after {key}", lambda: len(plays_after(vm, at)) >= 1)
    time.sleep(SETTLE_S)
    return at


def test_full_plays_mode_tab_and_highlight(board, kernel_image, card, tmp_path, need_mtools):
    sound_on(card, tmp_path)
    write_settings(card, tmp_path, ui_sound=2)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
        move_and_select(vm)
        wait_for(vm, "wanted three table plays", lambda: len(plays_after(vm, 0)) >= 3)
        time.sleep(SETTLE_S)
        # BACK closes the detail: highlight.
        at = step(vm, "back", "back", HIGHLIGHT)
        assert plays_after(vm, at) == [HIGHLIGHT], vm.serial[at:]
        assert tones_after(vm, at) == [], vm.serial[at:]
        # RIGHT changes the category, a sub-tab: tab, not scroll.
        at = step(vm, "right", "cat=1", TAB)
        assert plays_after(vm, at) == [TAB], vm.serial[at:]
        assert tones_after(vm, at) == [], vm.serial[at:]
        # UP at the top is denied: the 180 Hz tone and no table.
        at = step(vm, "up", "end row=0", None)
        assert tones_after(vm, at) == [180], vm.serial[at:]
        assert plays_after(vm, at) == [], vm.serial[at:]
        # LEFT back to the first category: tab again.
        at = step(vm, "left", "cat=0", TAB)
        assert plays_after(vm, at) == [TAB], vm.serial[at:]
        # LEFT on the first category changes the tab: mode.
        at = len(vm.serial)
        press(vm, "left")
        assert wait_tabs(vm, 2)[-1] == "STAT", vm.serial
        wait_for(vm, "wanted the mode play", lambda: len(plays_after(vm, at)) >= 1)
        time.sleep(SETTLE_S)
        assert plays_after(vm, at) == [MODE], vm.serial[at:]
        assert tones_after(vm, at) == [], vm.serial[at:]
        no_exception(vm)


def test_full_keeps_the_tone_for_a_count_change(board, kernel_image, card, tmp_path, need_mtools):
    """Scroll is a list row move only. A count change is another move and
    keeps the move tone."""
    sound_on(card, tmp_path)
    write_settings(card, tmp_path, ui_sound=2)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
        move_and_select(vm)
        wait_for(vm, "wanted two table plays", lambda: len(plays_after(vm, 0)) >= 2)
        time.sleep(SETTLE_S)
        at = len(vm.serial)
        inv_key(vm, "up", "count=")
        wait_for(vm, "wanted the move tone", lambda: len(tones_after(vm, at)) >= 1)
        time.sleep(SETTLE_S)
        assert tones_after(vm, at) == [1400], vm.serial[at:]
        assert SCROLL not in plays_after(vm, at), vm.serial[at:]
        no_exception(vm)


def test_on_keeps_the_tones(board, kernel_image, card, tmp_path, need_mtools):
    sound_on(card, tmp_path)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        at = move_and_select(vm)
        wait_for(vm, "wanted three tones", lambda: len(tones_after(vm, at)) >= 3)
        time.sleep(SETTLE_S)
        assert tones_after(vm, at) == [1400, 1400, 1900], vm.serial[at:]
        assert plays_after(vm, at) == [], vm.serial[at:]
        no_exception(vm)


def to_sound_row(vm):
    script(vm, "dial -1")
    assert wait_tabs(vm, 1)[-1] == "RADIO", vm.serial
    for row in range(1, ROW_SOUND + 1):
        assert radio_key(vm, "down") == f"row={row}", vm.serial


def test_row_offers_full_with_the_sound_on(board, kernel_image, card, tmp_path, need_mtools):
    sound_on(card, tmp_path)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_sound_row(vm)
        assert [radio_key(vm, "select") for _ in range(3)] == \
            ["sound=2", "sound=0", "sound=1"], vm.serial
        no_exception(vm)


def test_no_full_with_the_sound_off(board, kernel_image, card, tmp_path, need_mtools):
    write_settings(card, tmp_path, ui_sound=2)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
        # A saved Full plays the tones.
        at = move_and_select(vm)
        wait_for(vm, "wanted three tones", lambda: len(tones_after(vm, at)) >= 3)
        time.sleep(SETTLE_S)
        assert tones_after(vm, at) == [1400, 1400, 1900], vm.serial[at:]
        assert "rate=37500" not in vm.serial, vm.serial
        # The row toggles Off and On only.
        inv_key(vm, "back", "back")
        script(vm, "dial -1")
        script(vm, "dial -1")
        assert wait_tabs(vm, 3)[-1] == "RADIO", vm.serial
        for row in range(1, ROW_SOUND + 1):
            assert radio_key(vm, "down") == f"row={row}", vm.serial
        assert [radio_key(vm, "select") for _ in range(3)] == \
            ["sound=0", "sound=1", "sound=0"], vm.serial
        no_exception(vm)
