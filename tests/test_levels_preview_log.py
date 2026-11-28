"""Saves the serial log of one session at set sound levels, for `make levels-preview`.

It runs only with --levels-preview-log PATH and --levels-preview NAME.
NAME picks one row of LEVEL_SETS. The card is the one make sdimage
writes, its PIPOS.CFG set as for make boot-sound, with the master at 80
and the four levels of the row under [audio]. So each session starts at
its levels with no input and no card write.

Six names run the level session. "UI sounds" is on Full, with no hum:

- the boot, up to the weep
- the dial to INV, RIGHT, SELECT, BACK, the dial back to RADIO
- DOWN and UP on RADIO
- Morning, a tune, for 5 s
- Hearth, a recorded station, for 5 s
- DATA, TAPES and Culvert: level 1 solved with its tick, its connect
  notes and its solved tune, then Quit

"sweep" runs the sweep session, every level at 100 and "UI sounds" off:
Morning plays while the radio level walks from 100 down to 0 and back
up to 100 in steps of 5, in the levels view.

Each session then checks the audio trace: that each kind of sound played
at its level, and that nothing else changed. RobCo-Core's
tools/audio_preview.py turns the log into a WAV file.
"""

import re
import time

import pytest

from conftest import _built
from pipos_testing import TIMEOUT, config_summary, no_exception
from test_boot_sound_log import CFG, CFG_PATH, wait_quiet
from test_culvert import ended, finish, go, key as tape_key, launch, quit_from_play, solution
from test_data import to_tapes
from test_deitrix import boot_rom, wait_ready
from test_model import card_target, mtools, wait_line
from test_radio_levels import fields, gain, open_view
from test_radio_tab import UI_HZ, key, to_radio, walk
from test_sfx import HIGHLIGHT, MODE, SCROLL, SELECT, TAB, plays_after
from test_sfx_preview_log import GAP_S, dial, keyed, wait_for
from test_sfx_tape_tuner import TAPE_START, TAPE_STOP, TUNER
from test_weep import WEEP, write_settings

MASTER = 80
# The radio, effects, recordings and games levels of each file.
LEVEL_SETS = {
    "before": (100, 100, 100, 100),
    "default": (40, 100, 80, 50),
    "radio-30": (30, 100, 80, 50),
    "radio-50": (50, 100, 80, 50),
    "games-40": (40, 100, 80, 40),
    "games-70": (40, 100, 80, 70),
    "sweep": (100, 100, 100, 100),
}
KINDS = ("radio", "sfx", "recordings", "games")
STATION_S = 5.0
TURN_S = 0.3
TUNE_AFTER_S = 2.0
STEP_S = 0.25
HOLD_S = 6.0
# Culvert's sounds: the tick, the two connect notes, the solved tune.
TICK_HZ = 900
CONNECT_HZ = [1320, 1760]
SOLVED_HZ = [784, 988, 1175]
LINE = re.compile(r"^(@RC stage=pipos ev=audio op=(\w+) (.*?))\r?$", re.M)
SWEEP_STEPS = list(range(95, -1, -5)) + list(range(5, 101, 5))


def audio_lines(log, start=0, end=None):
    """[(offset, op, {field: value})] of every audio line in log[start:end]."""
    end = len(log) if end is None else end
    out = []
    for m in LINE.finditer(log, start):
        if m.start() >= end:
            break
        out.append((m.start(), m[2], dict(f.split("=", 1) for f in m[3].split() if "=" in f)))
    return out


def shown(f):
    """The vol= value and the gain= tail of a start line's fields."""
    return f["vol"], f" gain={f['gain']}" if "gain" in f else ""


def write_cfg(card, tmp_path, levels):
    cfg = tmp_path / "PIPOS.CFG"
    cfg.write_text(CFG + "".join(f"{k} = {v}\n" for k, v in zip(KINDS, levels)))
    mtools("mcopy", "-o", "-i", card_target(card), str(cfg), CFG_PATH)


def check_start(vm, levels):
    summary = config_summary(vm)
    got = tuple(summary.get(k) for k in ("volume",) + KINDS)
    want = (str(MASTER),) + tuple(str(v) for v in levels)
    assert got == want, f"the config summary shows {got}, not {want}\n{vm.serial}"
    assert " ev=levels " not in vm.serial, f"the boot printed ev=levels\n{vm.serial}"


def solve_level_1(vm, solver):
    """The turns of the solver's solution, with a pause after each."""
    (_, _, sx, sy), turns = solution(solver, 1)
    pos = (sx, sy)
    moves = 0
    for x, y, n in turns:
        pos = go(vm, pos, x, y)
        for _ in range(n):
            moves += 1
            got = tape_key(vm, "select", rf"turn x={x} y={y} connected=\d+|solved level=1 "
                                         rf"moves={moves}")
            time.sleep(TURN_S)
            if got.startswith("solved"):
                return
    pytest.fail(f"level 1 not solved after {moves} moves\n{vm.serial}")


def in_a_row(hzs, run):
    return any(hzs[i:i + len(run)] == run for i in range(len(hzs)))


def level_session(vm, solver):
    wait_ready(vm)
    wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=2 station=0")
    vm.wait_for(WEEP, timeout=TIMEOUT)
    time.sleep(GAP_S)
    # Keys on INV, then back to RADIO.
    dial(vm, 1, "INV", 1)
    keyed(vm, "right", "deitrix: inv cat=1")
    keyed(vm, "select", "deitrix: inv open=")
    keyed(vm, "back", "deitrix: inv back")
    dial(vm, -2, "RADIO", 2)
    keyed(vm, "down", "deitrix: radio row=1")
    keyed(vm, "up", "deitrix: radio row=0")
    # Morning, a tune.
    keyed(vm, "select", "deitrix: radio station=1")
    at = vm.serial.rindex("deitrix: radio station=1")
    wait_for(vm, "no tune after station 1",
             lambda: [a for a in audio_lines(vm.serial, at)
                      if a[1] == "tone" and int(a[2]["hz"]) not in UI_HZ])
    time.sleep(STATION_S)
    # Hearth, a recorded station. The radio blocks the UI sounds on the walk.
    walk(vm, 0, 3)
    key(vm, "select", "station=4")
    at = vm.serial.rindex("deitrix: radio station=4")
    wait_for(vm, "no recording after station 4",
             lambda: "ev=audio op=pcm rate=8000 " in vm.serial[at:])
    time.sleep(STATION_S)
    # Culvert. The launch stops the radio, then the tape start plays.
    to_tapes(vm, "dial 3")
    launch(vm)
    tape_key(vm, "select", r"level n=1 .*")
    solve_level_1(vm, solver)
    finish(vm)
    wait_line(vm, "culvert: save rc=30")
    quit_from_play(vm)
    ended(vm)
    back = vm.serial.index("deitrix: data tape rc=0")
    wait_for(vm, "no tape stop after the tape", lambda: TAPE_STOP in plays_after(vm, back))
    wait_quiet(vm)


def check_level_log(vm, levels):
    log = vm.serial
    radio, sfx, recordings, games = levels
    lines = audio_lines(log)
    station = log.index("deitrix: radio station=1")
    holo = log.index("ev=holo_start")
    holo_end = log.index("ev=holo_exit")
    # The tune.
    tune = [f for at, op, f in lines
            if station <= at < holo and op == "tone" and int(f["hz"]) not in UI_HZ]
    if len(tune) < 6:
        pytest.fail(f"the tune played {len(tune)} notes, fewer than 6\n{log[station:holo]}")
    bad = [f for f in tune if shown(f) != fields(MASTER, radio)]
    assert not bad, f"tune notes not at radio level {radio}: {bad}"
    # The recording.
    rec = [f for _, op, f in lines if op == "pcm" and f["rate"] == "8000"]
    if not rec:
        pytest.fail(f"the recording never played\n{log}")
    bad = [f for f in rec if shown(f) != fields(MASTER, recordings)]
    assert not bad, f"recording plays not at recordings level {recordings}: {bad}"
    # The tape.
    tape = [f for at, op, f in lines if holo <= at < holo_end and op == "tone"]
    hzs = [int(f["hz"]) for f in tape]
    for what, run in (("tick", [TICK_HZ]), ("connect notes", CONNECT_HZ),
                      ("solved tune", SOLVED_HZ)):
        if not in_a_row(hzs, run):
            pytest.fail(f"the tape played no {what} {run}: {hzs}")
    bad = [f for f in tape if shown(f) != fields(MASTER, games)]
    assert not bad, f"tape tones not at games level {games}: {bad}"
    # The effects.
    assert sfx == 100
    bad = [f for _, op, f in lines
           if op == "pcm" and f["rate"] == "37500" and shown(f) != (str(MASTER), "")]
    assert not bad, f"effects not at vol={MASTER} with no gain: {bad}"
    got = plays_after(vm, 0)
    for name, table in (("mode", MODE), ("tab", TAB), ("select", SELECT),
                        ("highlight", HIGHLIGHT), ("scroll", SCROLL),
                        ("tape start", TAPE_START), ("tape stop", TAPE_STOP)):
        if table not in got:
            pytest.fail(f"the session played no {name}\n{log}")
    if got.count(TUNER) < 2:
        pytest.fail(f"the session played the tuner {got.count(TUNER)} times, not twice\n{log}")
    assert WEEP.search(log), f"the session played no weep\n{log}"
    # Nothing else moved.
    assert " op=gain " not in log, f"a level changed during a play\n{log}"
    assert "deitrix: save PIPOS.SET" not in log, f"the session stored levels\n{log}"


def sweep_session(vm):
    wait_ready(vm)
    wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=0 station=0")
    wait_quiet(vm)
    to_radio(vm)
    key(vm, "select", "station=1")
    at = vm.serial.rindex("deitrix: radio station=1")
    wait_for(vm, "no tune after station 1",
             lambda: [a for a in audio_lines(vm.serial, at)
                      if a[1] == "tone" and int(a[2]["hz"]) not in UI_HZ])
    time.sleep(TUNE_AFTER_S)
    open_view(vm, 0, dict(zip(KINDS, LEVEL_SETS["sweep"])))
    for i, n in enumerate(SWEEP_STEPS):
        key(vm, "left" if i < 20 else "right", f"level radio={n}")
        time.sleep(STEP_S)
    time.sleep(HOLD_S)
    key(vm, "select", "levels closed")
    walk(vm, 5, 0)
    key(vm, "select", "off")
    off = vm.serial.rindex("deitrix: radio off")
    wait_for(vm, "no stop after the radio went off", lambda: " op=stop " in vm.serial[off:])
    wait_quiet(vm)


def check_sweep_log(vm):
    log = vm.serial
    steps = [m[1] for m in re.finditer(r"^deitrix: radio level radio=(\d+)\r?$", log, re.M)]
    assert steps == [str(n) for n in SWEEP_STEPS], f"the steps were {steps}"
    first = log.index("deitrix: radio level radio=95")
    off = log.index("deitrix: radio off")
    lines = audio_lines(log, first, off)
    gains = [int(f["gain"]) for _, op, f in lines if op == "gain"]
    print(f"sweep: {len(gains)} op=gain lines")
    want = iter(gain(MASTER, n) for n in SWEEP_STEPS)
    assert all(g in want for g in gains), f"op=gain values out of step order: {gains}"
    assert len(gains) >= 20, f"only {len(gains)} op=gain lines in the sweep: {gains}"
    other = [(at, op, f) for at, op, f in lines if op not in ("tone", "stop", "gain")]
    assert not other, f"other audio in the sweep: {other}"
    tones = [f for _, op, f in lines if op == "tone"]
    ui = [f for f in tones if int(f["hz"]) in UI_HZ]
    assert not ui, f"UI tones in the sweep: {ui}"
    assert any(int(f["vol"]) < MASTER for f in tones), f"no note under vol={MASTER}: {tones}"
    last = [f for _, op, f in audio_lines(log, 0, off) if op == "tone"][-1]
    assert shown(last) == (str(MASTER), ""), f"the last note is {last}"


def test_save_levels_log(request, board, kernel_image, card, tmp_path, need_mtools):
    path = request.config.getoption("--levels-preview-log")
    name = request.config.getoption("--levels-preview")
    if name not in LEVEL_SETS:
        pytest.fail(f"--levels-preview must be one of {', '.join(LEVEL_SETS)}, not {name!r}")
    levels = LEVEL_SETS[name]
    write_cfg(card, tmp_path, levels)
    write_settings(card, tmp_path, ui_sound=0 if name == "sweep" else 2)
    solver = None if name == "sweep" else _built(request, "host", "culvert_solve")
    with boot_rom(kernel_image, board, sd=card) as vm:
        if name == "sweep":
            sweep_session(vm)
        else:
            level_session(vm, solver)
        no_exception(vm)
        log = vm.serial
        check_start(vm, levels)
        with open(path, "w", encoding="utf-8") as f:
            f.write(log)
        if name == "sweep":
            check_sweep_log(vm)
        else:
            check_level_log(vm, levels)
