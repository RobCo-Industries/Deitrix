"""The kind of each sound in QEMU: Pip-OS plays each kind at its level.

QEMU has no sound device, so the tests read the audio trace. Pip-OS
computes one gain per play from the master, the level of the play's
kind and a trim, which is 100 for the ROM:

    gain = (master * level * level * trim + 5000) / 10000

A start line shows vol= as gain / 100 rounded down, and ends with
gain=<n> only when the gain is not a multiple of 100. A tune is radio, a
recorded station is recordings, the key tones are sfx and a holotape's
sounds are games.
"""

import re
import subprocess
import time

from pipos_testing import TIMEOUT, minimal_env, no_exception, press
from test_culvert import ended, key as tape_key, launch, quit_from_play
from test_data import to_tapes
from test_deitrix import boot_rom, wait_ready
from test_model import card_target, mtools
from test_radio_tab import UI_HZ, key, to_radio, walk
from test_weep import SETTLE_S, sound_on

CFG = "::PIPBOY/PIPOS.CFG"
# The levels of the card that tells the kinds apart.
LEVELS = {"volume": 80, "radio": 40, "sfx": 100, "recordings": 50, "games": 30}
TONE = re.compile(r"^@RC stage=pipos ev=audio op=tone hz=(\d+) vol=(\d+) t_ms=(\d+)(.*?)\r?$", re.M)
PCM = re.compile(r"^@RC stage=pipos ev=audio op=pcm rate=(\d+) len=\d+ vol=(\d+) .*?\r?$", re.M)
AUDIO_LINE = re.compile(r"^@RC stage=pipos ev=audio .*?\r?$", re.M)
# WALK.TUN, station 2: 56 notes over 14530 ms. No note is a UI tone.
WALK_MS = 14530


def gain(kind):
    master = LEVELS["volume"]
    level = LEVELS[kind]
    return (master * level * level * 100 + 5000) // 10000


def fields(kind):
    """The vol= value and the gain= tail a start line of `kind` shows."""
    g = gain(kind)
    return str(g // 100), "" if g % 100 == 0 else f" gain={g}"


def set_levels(card, tmp_path, levels):
    """Writes the [audio] levels into the card's PIPOS.CFG."""
    text = subprocess.run(["mtype", "-i", card_target(card), CFG], check=True,
                          env=minimal_env(MTOOLS_SKIP_CHECK="1"),
                          capture_output=True, text=True).stdout
    text, n = re.subn(r"(?m)^volume *=.*\n", "", text)
    assert n == 1, text
    keys = "".join(f"{k} = {v}\n" for k, v in levels.items())
    text, n = re.subn(r"(?m)^\[audio\]\n", "[audio]\n" + keys, text)
    assert n == 1, text
    cfg = tmp_path / "PIPOS.CFG"
    cfg.write_text(text)
    mtools("mcopy", "-o", "-i", card_target(card), str(cfg), CFG)


def tones_after(vm, at):
    """[(hz, vol, t_ms, tail)] of every op=tone line after offset `at`."""
    return [(int(m[1]), m[2], int(m[3]), m[4]) for m in TONE.finditer(vm.serial, at)]


def wait_for(cond, what, vm):
    deadline = time.monotonic() + TIMEOUT
    while True:
        got = cond()
        if got:
            return got
        assert time.monotonic() < deadline, f"{what}\n{vm.serial}"
        time.sleep(0.1)


def ui_tone_after(vm, at):
    """Waits for the first UI tone after offset `at`, returns it."""
    return wait_for(lambda: [t for t in tones_after(vm, at) if t[0] in UI_HZ],
                    "no UI tone", vm)[0]


def stop_and_tone(vm, row):
    """SELECT on the playing station turns the radio off, then DOWN plays
    the move tone. Returns that tone."""
    key(vm, "select", "off")
    at = vm.serial.rindex("deitrix: radio off")
    key(vm, "down", f"row={row + 1}")
    return ui_tone_after(vm, at)


def test_levels_tell_the_kinds_apart(board, kernel_image, card, tmp_path, need_mtools):
    set_levels(card, tmp_path, LEVELS)
    radio_vol, radio_tail = fields("radio")
    rec_vol, rec_tail = fields("recordings")
    sfx_vol, sfx_tail = fields("sfx")
    assert (radio_vol, radio_tail) == ("12", " gain=1280")
    assert (rec_vol, rec_tail) == ("20", "")
    assert (sfx_vol, sfx_tail) == ("80", "")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        # A tune, long enough to start again.
        walk(vm, 0, 1)
        key(vm, "select", "station=2")
        at = vm.serial.rindex("deitrix: radio station=2")

        def again():
            got = tones_after(vm, at)
            return got if got and got[-1][2] >= got[0][2] + WALK_MS else None

        tune = wait_for(again, "the tune did not start again", vm)
        assert all((v, tail) == (radio_vol, radio_tail) for _, v, _, tail in tune), tune
        tone = stop_and_tone(vm, 1)
        assert (tone[1], tone[3]) == (sfx_vol, sfx_tail), tone
        # A recorded station.
        walk(vm, 2, 3)
        key(vm, "select", "station=4")
        at = vm.serial.rindex("deitrix: radio station=4")
        pcm = wait_for(lambda: [m for m in PCM.finditer(vm.serial, at) if m[1] == "8000"],
                       "no op=pcm of the station", vm)[0]
        assert pcm[2] == rec_vol, pcm[0]
        assert " gain=" not in pcm[0], pcm[0]
        tone = stop_and_tone(vm, 3)
        assert (tone[1], tone[3]) == (sfx_vol, sfx_tail), tone
        no_exception(vm)


def test_holotape_plays_as_games(board, kernel_image, card, tmp_path, need_mtools):
    set_levels(card, tmp_path, LEVELS)
    games_vol, games_tail = fields("games")
    sfx_vol, sfx_tail = fields("sfx")
    assert (games_vol, games_tail) == ("7", " gain=720")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_tapes(vm, "dial 2")
        launch(vm)
        start = vm.serial.index("ev=holo_start")
        tape_key(vm, "select", r"level n=1 .*")
        # A turn plays the tape's tick tone.
        tape_key(vm, "select", r"turn .*")
        wait_for(lambda: tones_after(vm, start), "no tone from the tape", vm)
        quit_from_play(vm)
        ended(vm)
        end = vm.serial.index("ev=holo_exit")
        tape = [m for m in TONE.finditer(vm.serial, start) if m.start() < end]
        assert tape, vm.serial[start:end]
        assert all((m[2], m[4]) == (games_vol, games_tail) for m in tape), [m[0] for m in tape]
        # Back in Deitrix: a key tone is sfx again.
        mark = len(vm.serial)
        press(vm, "down")
        tone = ui_tone_after(vm, mark)
        assert (tone[1], tone[3]) == (sfx_vol, sfx_tail), tone
        no_exception(vm)


def test_full_level_prints_no_gain(board, kernel_image, card, need_mtools):
    """The stock card: every level at 100, so no line carries a gain."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_radio(vm)
        key(vm, "select", "station=1")
        at = vm.serial.rindex("deitrix: radio station=1")
        wait_for(lambda: tones_after(vm, at), "no tune", vm)
        stop_and_tone(vm, 0)
        walk(vm, 1, 3)
        key(vm, "select", "station=4")
        at = vm.serial.rindex("deitrix: radio station=4")
        wait_for(lambda: [m for m in PCM.finditer(vm.serial, at) if m[1] == "8000"],
                 "no op=pcm of the station", vm)
        stop_and_tone(vm, 3)
        time.sleep(SETTLE_S)
        lines = AUDIO_LINE.findall(vm.serial)
        assert lines
        assert not [ln for ln in lines if " gain=" in ln or " op=gain " in ln], lines
        no_exception(vm)


def test_start_prints_no_gain_change(board, kernel_image, card, tmp_path, need_mtools):
    """With the system sound on, the start reads the level and changes
    nothing, so no op=gain line comes from power-on to after the start."""
    sound_on(card, tmp_path)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        time.sleep(SETTLE_S)
        assert " op=gain " not in vm.serial, vm.serial
        no_exception(vm)
