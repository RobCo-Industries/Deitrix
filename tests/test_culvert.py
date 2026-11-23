"""The Culvert holotape in QEMU, started from Deitrix as a user does.

Each test boots the Deitrix ROM with the default card, which holds
CULVERT.HTP. It goes to DATA, then TAPES, and SELECT runs the tape.
When the tape ends, Deitrix shows the TAPES list again with the result.

The tape logs one "culvert: ..." line per step, after the frame is on
screen. So a test that waits for the line of a key sees its frame.
The status row shows the time, so the goldens leave its right end out.
"""

import re
import subprocess
import time

import pytest
from PIL import ImageDraw

from conftest import ROOT, _built, serial_line

from test_data import TAPE_RC, TAPES_LINE, to_tapes
from test_deitrix import boot_rom, golden, rom_running, wait_ready, wait_tabs
from test_inv import change_knife
from test_model import AUDIO, card_target, mtools, wait_line
from test_stat import count, settled, wait_count
from pipos_testing import TIMEOUT, minimal_env, no_exception, press, screenshot, script

LINE = re.compile(r"^culvert: (.*?)\r?\n", re.M)
AUDIO_TONE = re.compile(r"^@RC stage=pipos ev=audio op=tone hz=(\d+)", re.M)
SHOWN = serial_line(r"deitrix: radio shows (.*?)")
RADIO_KEY = serial_line(r"deitrix: radio (.*?)")
SAVE_OK = serial_line(r"deitrix: save (\S+) ok")
DEITRIX_LOAD = serial_line(r"deitrix: load DEITRIX\.SAV ok tab=(\d+) volume=(\d+) sound=(\d+) "
                           r"station=(\d+)")
# The UI tones of src/ui/core.c: MOVE, SELECT, BACK, DENY.
UI_HZ = {1400, 1900, 900, 180}
# The time, right of the moves in the status row, in screen pixels.
TIME_BOX = (480, 426, 640, 480)
LISTED = ("0", "0", "1", "")
ENDED = ("0", "0", "1", "Tape ended, status 0.")
# RADIO rows on the sample card: 0 to 4 the stations, 6 UI sounds.
ROW_SOUND = 6


@pytest.fixture
def solver(request):
    return _built(request, "host", "culvert_solve")


def solution(solver, level):
    """(w, h, sx, sy) and the turns that solve `level`, from logic.c."""
    out = subprocess.run([solver, str(level)], check=True, capture_output=True, text=True,
                         env=minimal_env(), cwd=ROOT).stdout.split("\n")
    size = tuple(int(v) for v in out[0].split()[1:])
    turns = [tuple(int(v) for v in line.split()[1:]) for line in out[1:] if line]
    return size, turns


def lines(vm):
    return LINE.findall(vm.serial)


def wait_next(vm, before):
    deadline = time.monotonic() + TIMEOUT
    while len(lines(vm)) <= before:
        assert time.monotonic() < deadline, f"no culvert line\n{vm.serial}"
        time.sleep(0.02)
    return lines(vm)[before]


def key(vm, name, want):
    """Taps a key, waits for the tape's next line and matches it with the
    regular expression `want`."""
    before = len(lines(vm))
    if name.startswith("dial "):
        script(vm, name)
    else:
        press(vm, name)
    got = wait_next(vm, before)
    assert re.fullmatch(want, got), f"{name}: {got!r} !~ {want!r}\n{vm.serial}"
    return got


def radio_key(vm, name, want):
    before = count(vm, RADIO_KEY)
    press(vm, name)
    got = [k for k in wait_count(vm, RADIO_KEY, before + 1)[before:]
           if not k.startswith(("shows ", "underrun"))]
    while not got:
        time.sleep(0.05)
        got = [k for k in RADIO_KEY.findall(vm.serial)[before:]
               if not k.startswith(("shows ", "underrun"))]
    assert got[0] == want, f"{name}: {got[0]!r} != {want!r}\n{vm.serial}"


def shot(vm, tmp_path, name, mask_time=True):
    img = screenshot(vm, tmp_path)
    if mask_time:
        ImageDraw.Draw(img).rectangle(TIME_BOX, fill=(0, 0, 0))
    golden(img, name)
    return img


def launch(vm, load="load rc=-2", tmp_path=None, title_name="culvert_title"):
    """SELECT on the tape in TAPES; waits for the title."""
    before = len(lines(vm))
    press(vm, "select")
    assert wait_next(vm, before) == load, vm.serial
    assert wait_next(vm, before + 1) == "title", vm.serial
    if tmp_path is not None:
        shot(vm, tmp_path, title_name, mask_time=False)


def ended(vm, runs=1):
    """Waits for the TAPES list after the tape's end, with its result."""
    want = runs
    deadline = time.monotonic() + TIMEOUT
    while vm.serial.count("deitrix: data tape rc=0") < want:
        assert time.monotonic() < deadline, vm.serial
        time.sleep(0.05)
    at = [m.start() for m in re.finditer("deitrix: data tape rc=0", vm.serial)][want - 1]
    deadline = time.monotonic() + TIMEOUT
    while ENDED not in TAPES_LINE.findall(vm.serial[at:]):
        assert time.monotonic() < deadline, vm.serial[at:]
        time.sleep(0.05)
    assert TAPE_RC.findall(vm.serial)[-1] == "0", vm.serial


def quit_from_play(vm):
    key(vm, "back", "pause")
    key(vm, "down", "menu item=1")
    key(vm, "down", "menu item=2")
    key(vm, "select", "exit rc=0")


def go(vm, pos, x, y):
    """Moves the cursor from pos to (x, y) and returns the new pos."""
    cx, cy = pos
    while cx != x:
        step = 1 if x > cx else -1
        cx += step
        key(vm, "right" if step > 0 else "left", rf"cursor x={cx} y={cy}")
    while cy != y:
        step = 1 if y > cy else -1
        cy += step
        key(vm, "down" if step > 0 else "up", rf"cursor x={cx} y={cy}")
    return (cx, cy)


def solve(vm, solver, level, tmp_path=None, mid_name=None):
    """Plays the turns of the generator's solution. Returns the moves."""
    (w, h, sx, sy), turns = solution(solver, level)
    pos = (sx, sy)
    moves = 0
    for i, (x, y, n) in enumerate(turns):
        if mid_name and i == len(turns) // 2:
            shot(vm, tmp_path, mid_name)
        pos = go(vm, pos, x, y)
        for _ in range(n):
            moves += 1
            got = key(vm, "select", rf"turn x={x} y={y} connected=\d+|solved level={level} "
                                    rf"moves={moves}")
            if got.startswith("solved"):
                return moves
    raise AssertionError(f"level {level} not solved after {moves} moves\n{vm.serial}")


def tune_after(vm, at):
    """Asserts the solve tune plays after serial offset `at`.

    The kernel prints a tone's trace from its audio tick, so the tone of
    the last turn may land after the "solved" line. What holds is that the
    tune's three notes play in a row: the first 784 Hz after `at`, then
    988 Hz and 1175 Hz, with no other tone between them."""
    deadline = time.monotonic() + TIMEOUT
    while True:
        tones = [int(hz) for hz in AUDIO_TONE.findall(vm.serial[at:])]
        if 784 in tones and len(tones) >= tones.index(784) + 3:
            break
        assert time.monotonic() < deadline, vm.serial[at:]
        time.sleep(0.05)
    first = tones.index(784)
    assert tones[first:first + 3] == [784, 988, 1175], vm.serial[at:]


def finish(vm):
    """Waits for the end of the flourish."""
    wait_line(vm, "culvert: done")


def test_culvert_play(board, kernel_image, card, solver, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        # A station plays first: RADIO is one detent back from STAT.
        script(vm, "dial -1")
        wait_line(vm, "deitrix: tab=RADIO")
        press(vm, "select")
        wait_line(vm, "deitrix: radio station=1")
        assert to_tapes(vm, "dial 3") == LISTED
        golden(settled(vm, tmp_path), "culvert_tapes")
        mark = len(vm.serial)
        launch(vm, tmp_path=tmp_path)
        start = vm.serial.index("ev=holo_start", mark)
        assert "deitrix: data launch CULVERT.HTP" in vm.serial[mark:start], vm.serial
        # The title: UP at the top changes nothing, DOWN moves to Quit and back.
        key(vm, "up", "idle")
        key(vm, "down", "menu item=1")
        key(vm, "up", "menu item=0")
        seed = key(vm, "select", r"level n=1 w=5 h=4 seed=0x[0-9a-f]{8}")
        shot(vm, tmp_path, "culvert_level1")
        # LEFT at the edge stops; a held SELECT turns once.
        key(vm, "left", "cursor x=1 y=2")
        key(vm, "left", "cursor x=0 y=2")
        key(vm, "left", "idle")
        key(vm, "right", "cursor x=1 y=2")
        key(vm, "right", "cursor x=2 y=2")
        before = len(lines(vm))
        script(vm, "key select 1")
        time.sleep(1.5)
        script(vm, "key select 0")
        got = wait_next(vm, before)
        assert re.fullmatch(r"turn x=2 y=2 connected=\d+", got), vm.serial
        time.sleep(0.3)
        assert len(lines(vm)) == before + 1, vm.serial
        # The dial turns back: three quarters clockwise undo the press.
        key(vm, "dial 3", r"turn x=2 y=2 connected=\d+")
        # Restart from the pause menu brings the level back as generated.
        key(vm, "back", "pause")
        shot(vm, tmp_path, "culvert_pause")
        key(vm, "down", "menu item=1")
        assert key(vm, "select", r"level n=1 .*") == seed, vm.serial
        moves = solve(vm, solver, 1, tmp_path, "culvert_mid")
        assert moves > 0
        solved = vm.serial.rindex("culvert: solved")
        finish(vm)
        shot(vm, tmp_path, "culvert_solved")
        # The radio stopped for the tape, and the tape's own tune plays.
        ops = [m[1] for m in AUDIO.finditer(vm.serial, mark)]
        assert "stop" in ops, vm.serial[mark:]
        tune_after(vm, solved)
        wait_line(vm, "culvert: save rc=30")
        key(vm, "select", r"level n=2 w=5 h=4 seed=0x[0-9a-f]{8}")
        # Pause, then Quit: the tape ends with 0 and Deitrix shows TAPES.
        key(vm, "back", "pause")
        key(vm, "down", "menu item=1")
        key(vm, "down", "menu item=2")
        key(vm, "down", "idle")
        key(vm, "select", "exit rc=0")
        ended(vm)
        golden(settled(vm, tmp_path), "culvert_tapes_return")
        rom_running(vm)
        draws = re.search(r"^culvert stats: grid draws=(\d+) ms=(\d+)\r?$", vm.serial, re.M)
        assert draws and int(draws.group(1)) > 0, vm.serial
        print(f"{board}: {draws.group(0).strip()}")
        # The radio did not start again, and RADIO says it is off.
        after = vm.serial.index("deitrix: data tape rc=0")
        assert "op=seq" not in vm.serial[after:], vm.serial[after:]
        before = count(vm, SHOWN)
        script(vm, "dial 2")
        assert wait_count(vm, SHOWN, before + 1)[-1] == "Off", vm.serial
        no_exception(vm)


def test_culvert_dial_solve(board, kernel_image, card, solver, need_mtools):
    """Level 1 played with the dial: the push starts the game, the first
    tile turns by a dial event, and the last tile gets a two-step event
    whose first quarter solves the level. The second step is dropped. The
    push then starts level 2."""
    (w, h, sx, sy), turns = solution(solver, 1)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_tapes(vm) == LISTED
        launch(vm)
        key(vm, "dial_push", r"level n=1 w=5 h=4 seed=0x[0-9a-f]{8}")
        pos = (sx, sy)
        moves = 0
        for i, (x, y, n) in enumerate(turns):
            pos = go(vm, pos, x, y)
            if i == 0 and len(turns) > 1:
                moves += n
                key(vm, f"dial {n}", rf"turn x={x} y={y} connected=\d+")
            elif i < len(turns) - 1:
                for _ in range(n):
                    moves += 1
                    key(vm, "select", rf"turn x={x} y={y} connected=\d+")
            else:
                # One quarter before the solution, then two steps.
                for _ in range(n - 1):
                    moves += 1
                    key(vm, "select", rf"turn x={x} y={y} connected=\d+")
                moves += 1
                key(vm, "dial 2", rf"solved level=1 moves={moves}")
        solved = vm.serial.rindex("culvert: solved")
        tune_after(vm, solved)
        finish(vm)
        key(vm, "dial_push", r"level n=2 w=5 h=4 seed=0x[0-9a-f]{8}")
        quit_from_play(vm)
        ended(vm)
        no_exception(vm)


def test_culvert_save(board, kernel_image, card, solver, tmp_path, need_mtools):
    """Deitrix's saves and the tape's save live side by side: a count and
    UI sounds changed before the launch hold after the tape's save, after
    the return and after a reboot; the tape's next start offers level 2."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        script(vm, "dial 1")
        assert wait_tabs(vm, 1)[-1] == "INV", vm.serial
        change_knife(vm)
        script(vm, "dial -2")
        assert wait_tabs(vm, 2)[-1] == "RADIO", vm.serial
        for row in range(1, ROW_SOUND + 1):
            radio_key(vm, "down", f"row={row}")
        radio_key(vm, "select", "sound=0")
        assert to_tapes(vm, "dial 3") == LISTED
        launch(vm)
        # The launch flushed both of Deitrix's saves first.
        launched = vm.serial.index("deitrix: data launch CULVERT.HTP")
        assert set(SAVE_OK.findall(vm.serial[:launched])) >= {"DEITRIX.SAV", "INV.SAV"}, \
            vm.serial
        key(vm, "select", r"level n=1 .*")
        solve(vm, solver, 1)
        wait_line(vm, "culvert: save rc=30")
        finish(vm)
        quit_from_play(vm)
        ended(vm)
        # UI sounds are still off: a walk to RADIO plays no UI tone.
        mark = len(vm.serial)
        script(vm, "dial 2")
        assert wait_tabs(vm, 4)[-1] == "RADIO", vm.serial
        radio_key(vm, "up", "row=5")
        script(vm, "state")
        time.sleep(0.5)
        tones = {int(hz) for hz in AUDIO_TONE.findall(vm.serial[mark:])}
        assert not tones & UI_HZ, vm.serial[mark:]
        # Back to DATA, which kept TAPES. Started again: the save loads and
        # the title offers level 2.
        before = count(vm, TAPES_LINE)
        script(vm, "dial 3")
        assert wait_tabs(vm, 5)[-1] == "DATA", vm.serial
        assert wait_count(vm, TAPES_LINE, before + 1)[-1][:3] == LISTED[:3], vm.serial
        launch(vm, load="load rc=30", tmp_path=tmp_path, title_name="culvert_title_continue")
        key(vm, "select", r"level n=2 .*")
        quit_from_play(vm)
        ended(vm, runs=2)
        no_exception(vm)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert DEITRIX_LOAD.findall(vm.serial)[0][2] == "0", vm.serial
        wait_line(vm, "deitrix: load INV.SAV ok records=14")
        # The boot opens on DATA, the last tab; RIGHT shows TAPES.
        before = count(vm, TAPES_LINE)
        press(vm, "right")
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == LISTED, vm.serial
        launch(vm, load="load rc=30")
        key(vm, "down", "menu item=1")
        key(vm, "down", "menu item=2")
        key(vm, "select", "exit rc=0")
        ended(vm)
        no_exception(vm)


def test_culvert_held_select_at_launch(board, kernel_image, card, need_mtools):
    """SELECT held for 1.5 s on the tape starts it once, and the held key
    neither starts a game nor picks a menu item on the title."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_tapes(vm) == LISTED
        script(vm, "key select 1")
        time.sleep(1.5)
        script(vm, "key select 0")
        wait_line(vm, "culvert: title")
        script(vm, "state")
        time.sleep(0.5)
        assert lines(vm) == ["load rc=-2", "title"], vm.serial
        assert vm.serial.count("ev=holo_start") == 1, vm.serial
        key(vm, "down", "menu item=1")
        key(vm, "select", "exit rc=0")
        ended(vm)
        no_exception(vm)


def test_culvert_corrupt_save(board, kernel_image, card, tmp_path, need_mtools):
    """A corrupt CULVERT.SAV starts fresh, with no error on screen."""
    bad = tmp_path / "CULVERT.SAV"
    bad.write_bytes(b"CVLT" + bytes([1, 9, 5, 0]) + bytes(18) + b"\x00\x00\x00\x00")
    target = card_target(card)
    subprocess.run(["mmd", "-i", target, "::/PIPBOY/SAVE"],
                   env=minimal_env(MTOOLS_SKIP_CHECK="1"), capture_output=True)
    mtools("mcopy", "-o", "-i", target, str(bad), "::/PIPBOY/SAVE/CULVERT.SAV")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_tapes(vm) == LISTED
        launch(vm, load="load rc=30", tmp_path=tmp_path)
        key(vm, "select", r"level n=1 .*")
        key(vm, "back", "pause")
        key(vm, "up", "idle")
        key(vm, "select", "resume")
        quit_from_play(vm)
        ended(vm)
        no_exception(vm)


def test_culvert_quit_from_title(board, kernel_image, card, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert to_tapes(vm) == LISTED
        launch(vm)
        key(vm, "down", "menu item=1")
        key(vm, "select", "exit rc=0")
        ended(vm)
        no_exception(vm)
