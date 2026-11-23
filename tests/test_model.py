"""Deitrix on Pip-OS in QEMU: the content it loads, the saves and the radio.

Each test boots a copy of the card from make sdimage. Some change the
copy with mtools first: a broken file, or no /PIPBOY/DATA at all. The
ROM logs one "deitrix: load" line per file, one "deitrix: save" line per
write and one "deitrix: radio" line per change of station. QEMU has no
PWM, so Pip-OS prints each change of sound as an audio trace line.
"""

import re
import subprocess
import time

from pipos_testing import TIMEOUT, booted, minimal_env, no_exception, script

READY_LINE = re.compile(r"^deitrix: ready\r?$", re.M)
TAB_LINE = re.compile(r"^deitrix: tab=(\w+)\r?$", re.M)
AUDIO = re.compile(r"^@RC stage=pipos ev=audio op=(\w+)(.*?)\r?$", re.M)
PART = 2048 * 512  # the card's FAT partition starts at LBA 2048

TUNES = ("MORNING", "WALK", "LULLABY")
CONTENT_LOG = [
    "deitrix: load PROFILE.INI gauges=4 attrs=6 about=3",
    "deitrix: load ITEMS.INI items=14 categories=4",
    "deitrix: load NOTES notes=4",
    "deitrix: load MAP.INI maps=2 lines=16 places=12",
    "deitrix: load RADIO.INI stations=5",
    *[re.compile(rf"deitrix: load RADIO/{t}\.TUN notes=\d+ ms=\d+") for t in TUNES],
    "deitrix: load RADIO/HEARTH.PCM bytes=256000",
    "deitrix: load RADIO/DRIFT.PCM bytes=256000",
]


def boot(image, board, sd):
    return booted(image, board, sd=sd, wait="jump")


def wait_ready(vm):
    vm.wait_for(READY_LINE, timeout=TIMEOUT)


def deitrix_lines(vm):
    return [l.rstrip("\r") for l in vm.serial.splitlines() if l.startswith("deitrix: ")]


def load_lines(vm):
    return [l for l in deitrix_lines(vm) if l.startswith("deitrix: load ")]


def matches(lines, want):
    assert len(lines) == len(want), "\n".join(lines)
    for line, w in zip(lines, want):
        if isinstance(w, str):
            assert line == w, "\n".join(lines)
        else:
            assert w.fullmatch(line), "\n".join(lines)


def wait_line(vm, text, timeout=TIMEOUT):
    vm.wait_for(re.compile("^" + re.escape(text) + r"\r?$", re.M), timeout=timeout)


def mtools(*args):
    subprocess.run(list(args), check=True, env=minimal_env(MTOOLS_SKIP_CHECK="1"),
                   capture_output=True)


def card_target(card):
    return f"{card}@@{PART}"


def to_radio(vm):
    """STAT to RADIO: one dial detent back."""
    script(vm, "dial -1")
    wait_line(vm, "deitrix: tab=RADIO")


def select(vm, line):
    script(vm, "key select 1")
    script(vm, "key select 0")
    wait_line(vm, line)


def audio_after(vm, mark):
    """The audio trace after the first serial line `mark`: [(op, rest)]."""
    at = vm.serial.index(mark)
    return [(m[1], m[2]) for m in AUDIO.finditer(vm.serial, at)]


def test_load_log(board, kernel_image, card, need_mtools):
    with boot(kernel_image, board, card) as vm:
        wait_ready(vm)
        matches(load_lines(vm), CONTENT_LOG + [
            re.compile(r"deitrix: load DEITRIX.SAV missing tab=0 volume=\d+ sound=1 station=0"),
            "deitrix: load INV.SAV missing",
        ])
        no_exception(vm)


def test_setting_survives_a_reboot(board, kernel_image, card, need_mtools):
    """RADIO's SELECT starts station 1, a tune: the trace shows its first
    note, G4 at 392 Hz. The autosave writes the tab and the station 2 s
    later. A second boot of the same card comes back on RADIO."""
    with boot(kernel_image, board, card) as vm:
        wait_ready(vm)
        to_radio(vm)
        select(vm, "deitrix: radio station=1")
        wait_line(vm, "deitrix: save DEITRIX.SAV ok", timeout=20)
        tones = [rest for op, rest in audio_after(vm, "deitrix: radio station=1") if op == "tone"]
        assert tones and " hz=392 " in tones[0], vm.serial
        assert "deitrix: save INV.SAV" not in vm.serial
        no_exception(vm)
    with boot(kernel_image, board, card) as vm:
        wait_ready(vm)
        assert any(re.fullmatch(r"deitrix: load DEITRIX.SAV ok tab=4 volume=\d+ sound=1 "
                                r"station=1", l) for l in load_lines(vm)), vm.serial
        # The first frame is RADIO's: no tab line, since no tab changed.
        assert TAB_LINE.findall(vm.serial) == [], vm.serial
        script(vm, "dial 1")
        wait_line(vm, "deitrix: tab=STAT")
        no_exception(vm)


def test_one_broken_file(board, kernel_image, card, tmp_path, need_mtools):
    bad = tmp_path / "MAP.INI"
    bad.write_bytes(b"version = 2\n[map.local]\nbox = 0,0,9,9\n")
    mtools("mcopy", "-o", "-i", card_target(card), str(bad), "::PIPBOY/DATA/MAP.INI")
    with boot(kernel_image, board, card) as vm:
        wait_ready(vm)
        want = list(CONTENT_LOG)
        want[3] = "deitrix: load MAP.INI ignored: bad version"
        matches(load_lines(vm)[:len(want)], want)
        no_exception(vm)


def test_no_data_folder(board, kernel_image, card, need_mtools):
    mtools("mdeltree", "-i", card_target(card), "::PIPBOY/DATA")
    with boot(kernel_image, board, card) as vm:
        wait_ready(vm)
        matches(load_lines(vm), [
            "deitrix: load PROFILE.INI missing",
            "deitrix: load ITEMS.INI missing",
            "deitrix: load NOTES missing",
            "deitrix: load MAP.INI missing",
            "deitrix: load RADIO.INI missing",
            re.compile(r"deitrix: load DEITRIX.SAV missing tab=0 volume=\d+ sound=1 station=0"),
            "deitrix: load INV.SAV missing",
        ])
        # Every tab draws its empty state. SELECT on RADIO does nothing:
        # the dial after it is the sync point, as the ROM takes events in order.
        for name in ("INV", "DATA", "MAP", "RADIO"):
            script(vm, "dial 1")
            wait_line(vm, f"deitrix: tab={name}")
        script(vm, "key select 1")
        script(vm, "key select 0")
        script(vm, "dial 1")
        wait_line(vm, "deitrix: tab=STAT")
        assert "deitrix: radio" not in vm.serial
        no_exception(vm)


def test_radio_streams_pcm(board, kernel_image, card, need_mtools):
    """Station 4 streams HEARTH.PCM in 8000-byte chunks: one op=pcm, then
    op=pcm_next for each chunk with no op=pcm_end between, so the queue
    never ran dry. A tab change does not stop it. Then SELECT twice: 5,
    then off, which stops the sound."""
    with boot(kernel_image, board, card) as vm:
        wait_ready(vm)
        to_radio(vm)
        for n in (1, 2, 3, 4):
            select(vm, f"deitrix: radio station={n}")
        mark = "deitrix: radio station=4"

        def ops():
            return [op for op, _ in audio_after(vm, mark)]

        deadline = time.monotonic() + 30
        while ops().count("pcm_next") < 2:
            assert time.monotonic() < deadline, vm.serial[vm.serial.index(mark):]
            time.sleep(0.1)
        got = ops()
        start = got.index("pcm")
        assert got[start:].count("pcm") == 1, got
        assert "pcm_end" not in got[start:] and "stop" not in got[start:], got
        assert all(" len=8000 " in rest for op, rest in audio_after(vm, mark)
                   if op in ("pcm", "pcm_next")), got
        # Change the tab while it plays: the stream goes on.
        script(vm, "dial 1")
        wait_line(vm, "deitrix: tab=STAT")
        before = ops().count("pcm_next")
        deadline = time.monotonic() + 30
        while ops().count("pcm_next") < before + 2:
            assert time.monotonic() < deadline, vm.serial[vm.serial.index(mark):]
            time.sleep(0.1)
        got = ops()
        assert got[start:].count("pcm") == 1, got
        assert "pcm_end" not in got and "stop" not in got and "tone" not in got[start:], got
        assert "deitrix: radio underrun" not in vm.serial
        script(vm, "dial -1")
        wait_line(vm, "deitrix: tab=RADIO")
        select(vm, "deitrix: radio station=5")
        select(vm, "deitrix: radio off")
        after = audio_after(vm, "deitrix: radio off")
        deadline = time.monotonic() + 10
        while "stop" not in [op for op, _ in audio_after(vm, "deitrix: radio station=5")]:
            assert time.monotonic() < deadline, vm.serial
            time.sleep(0.1)
        del after
        no_exception(vm)
