"""The Culvert holotape in QEMU: title, play, solve, pause, quit, the save.

Deitrix has no tape launcher yet, so these tests put the SDK's hello
example ROM on a copy of the card, as PIPBOY/ROM1.ROM. It lists the
tapes in /PIPBOY/HOLO and runs the one picked with SELECT, as a user
would. The card holds only CULVERT.HTP, so SELECT starts it.

The tape logs one "culvert: ..." line per step, after the frame is on
screen. So a test that waits for the line of a key sees its frame.
The status row shows the time, so the goldens leave its right end out.
"""

import re
import subprocess
import time

import pytest
from PIL import ImageDraw

from conftest import ROOT, _built

from test_deitrix import golden
from test_model import card_target, mtools, wait_line
from pipos_testing import TIMEOUT, booted, minimal_env, no_exception, press, screenshot, script

LINE = re.compile(r"^culvert: (.*?)\r?\n", re.M)
HELLO_LIST = re.compile(r"^hello: 1 holotapes on the card\r?$", re.M)
AUDIO_TONE = re.compile(r"^@RC stage=pipos ev=audio op=tone hz=(\d+)", re.M)
# The time, right of the moves in the status row, in screen pixels.
TIME_BOX = (480, 426, 640, 480)


@pytest.fixture
def tape_card(request, card, need_mtools):
    """The card with the hello ROM in place of Deitrix."""
    hello = _built(request, "hello", "ROM1.ROM")
    mtools("mcopy", "-o", "-i", card_target(card), hello, "::/PIPBOY/ROM1.ROM")
    return card


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
    if name.startswith("dial"):
        script(vm, name)
    else:
        press(vm, name)
    got = wait_next(vm, before)
    assert re.fullmatch(want, got), f"{name}: {got!r} !~ {want!r}\n{vm.serial}"
    return got


def shot(vm, tmp_path, name, mask_time=True):
    img = screenshot(vm, tmp_path)
    if mask_time:
        ImageDraw.Draw(img).rectangle(TIME_BOX, fill=(0, 0, 0))
    golden(img, name)
    return img


def launch(vm, title_name="culvert_title", tmp_path=None):
    """Picks the tape in hello and waits for the title."""
    before = len(lines(vm))
    press(vm, "select")
    assert wait_next(vm, before).startswith("load rc="), vm.serial
    assert wait_next(vm, before + 1) == "title", vm.serial
    if tmp_path is not None:
        shot(vm, tmp_path, title_name, mask_time=False)


def boot(kernel_image, board, card):
    vm = booted(kernel_image, board, sd=card, wait="jump")
    return vm


def wait_hello(vm):
    vm.wait_for(HELLO_LIST, timeout=TIMEOUT)


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


def finish(vm):
    """Waits for the end of the flourish."""
    wait_line(vm, "culvert: done")


def test_culvert_play(board, kernel_image, tape_card, solver, tmp_path):
    with boot(kernel_image, board, tape_card) as vm:
        wait_hello(vm)
        launch(vm, tmp_path=tmp_path)
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
        mark = vm.serial.rindex("culvert: solved")
        finish(vm)
        shot(vm, tmp_path, "culvert_solved")
        tones = [int(hz) for hz in AUDIO_TONE.findall(vm.serial[mark:])]
        assert tones[:3] == [784, 988, 1175], vm.serial[mark:]
        wait_line(vm, "culvert: save rc=30")
        key(vm, "select", r"level n=2 w=5 h=4 seed=0x[0-9a-f]{8}")
        # Pause, then Quit: the tape ends with 0 and hello redraws its list.
        key(vm, "back", "pause")
        key(vm, "down", "menu item=1")
        key(vm, "down", "menu item=2")
        key(vm, "down", "idle")
        key(vm, "select", "exit rc=0")
        wait_line(vm, "hello: CULVERT.HTP: exit status 0")
        draws = re.search(r"^culvert stats: grid draws=(\d+) ms=(\d+)\r?$", vm.serial, re.M)
        assert draws and int(draws.group(1)) > 0, vm.serial
        print(f"{board}: {draws.group(0).strip()}")
        no_exception(vm)


def test_culvert_save(board, kernel_image, tape_card, solver, tmp_path):
    """Solve level 1, quit; the next start offers Continue (level 2)."""
    with boot(kernel_image, board, tape_card) as vm:
        wait_hello(vm)
        launch(vm)
        key(vm, "select", r"level n=1 .*")
        solve(vm, solver, 1)
        wait_line(vm, "culvert: save rc=30")
        finish(vm)
        key(vm, "back", "pause")
        key(vm, "down", "menu item=1")
        key(vm, "down", "menu item=2")
        key(vm, "select", "exit rc=0")
        wait_line(vm, "hello: CULVERT.HTP: exit status 0")
        # Started again: the save loads and the title offers level 2.
        before = len(lines(vm))
        press(vm, "select")
        assert wait_next(vm, before) == "load rc=30", vm.serial
        assert wait_next(vm, before + 1) == "title", vm.serial
        shot(vm, tmp_path, "culvert_title_continue", mask_time=False)
        key(vm, "select", r"level n=2 .*")
        # Quit from the pause menu again.
        key(vm, "back", "pause")
        key(vm, "down", "menu item=1")
        key(vm, "down", "menu item=2")
        key(vm, "select", "exit rc=0")
        wait_line(vm, "hello: CULVERT.HTP: exit status 0", timeout=TIMEOUT)
        assert vm.serial.count("hello: CULVERT.HTP: exit status 0") == 2, vm.serial
        no_exception(vm)


def test_culvert_corrupt_save(board, kernel_image, tape_card, tmp_path):
    """A corrupt CULVERT.SAV starts fresh, with no error on screen."""
    bad = tmp_path / "CULVERT.SAV"
    bad.write_bytes(b"CVLT" + bytes([1, 9, 5, 0]) + bytes(18) + b"\x00\x00\x00\x00")
    target = card_target(tape_card)
    subprocess.run(["mmd", "-i", target, "::/PIPBOY/SAVE"],
                   env=minimal_env(MTOOLS_SKIP_CHECK="1"), capture_output=True)
    mtools("mcopy", "-o", "-i", target, str(bad), "::/PIPBOY/SAVE/CULVERT.SAV")
    with boot(kernel_image, board, tape_card) as vm:
        wait_hello(vm)
        launch(vm, tmp_path=tmp_path)
        assert "culvert: load rc=30" in vm.serial, vm.serial
        key(vm, "select", r"level n=1 .*")
        key(vm, "back", "pause")
        key(vm, "up", "idle")
        key(vm, "select", "resume")
        key(vm, "back", "pause")
        key(vm, "down", "menu item=1")
        key(vm, "down", "menu item=2")
        key(vm, "select", "exit rc=0")
        wait_line(vm, "hello: CULVERT.HTP: exit status 0")
        no_exception(vm)


def test_culvert_quit_from_title(board, kernel_image, tape_card):
    with boot(kernel_image, board, tape_card) as vm:
        wait_hello(vm)
        launch(vm)
        key(vm, "down", "menu item=1")
        key(vm, "select", "exit rc=0")
        wait_line(vm, "hello: CULVERT.HTP: exit status 0")
        no_exception(vm)
