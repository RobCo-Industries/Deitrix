# Testing

## Layers

| Layer | Command | What it covers |
|---|---|---|
| Host unit tests | `make test-host` | Pure code in `src/model/`, `src/ui/`, `src/screens/` and the app shell, against a fake platform; `tools/pbm2c.py` |
| Pin checks | `make check-pins`, part of `test-host` | The CI workflow pins and the commit links in the docs |
| Content check | `make check-content`, part of `test-host` | `build/card/` follows [CONTENT.md](CONTENT.md) |
| Tool tests | `make tools-test`, part of `test-host` | `tools/check_content.py`, `tools/tun2notes.py` and `tools/mkpcm.py` |
| Card check | `make sdimage-test`, part of `test-host` | The card holds the ROM, the kernel, `PIPOS.CFG` and every file of `build/card/` with the same bytes; `build/card/` holds every file of `content/` and both PCM tracks |
| QEMU tests | `make test-target` | The ROM and the widget gallery on Pip-OS on `raspi0`, `raspi1ap`, `raspi2b` and `raspi3b` |

`make test` runs `test-host`, then `test-target`. `BOARD=raspi0` limits the QEMU tests to one board.

## Host tests

Each `tests/host/test_<name>.c` builds into one program with RobCo-Core's `mk/host.mk`: `-Werror`, AddressSanitizer and UndefinedBehaviorSanitizer when the compiler has them. Every program links all of `src/` except `src/platform/`, the generated `build/gen/icons.c` and `tests/host/fake_platform.c`. The fake records draw calls, log lines and tones, and plays back scripted events, clock readings and ticks. It can also move its clock and tick counter forward at each `present`, to play a slow draw.

CI builds the host tests with GCC and `-Werror`, so GCC-only warnings such as `-Wformat-truncation` fail there. Apple clang does not report them. If a GCC is installed, run the same check with `make CI=1 test-host HOST_CC=gcc-15` (use your GCC's name).

| File | Covers |
|---|---|
| `test_nav.c` | Next and previous, wrap, several detents both ways, extreme deltas, any tab count |
| `test_clock.c` | `00:00`, `23:59`, `--:--`, the minute of the day, the wait to the next minute |
| `test_ui.c` | The theme shades, event translation, `dx_ui_is_back()`, the sound for each outcome, the tones, number text, cut text |
| `test_frame.c` | The header and footer draw lists, the active-tab block, the tab pad with many tabs, every label left of the clock for 1 to 8 tabs of 1 to 5 letters, the cut labels, the refused tables, the placeholder body, the ROM's screen table |
| `test_app.c` | The refused tab tables, one `present` per redraw, the clip around the body, no redraw without a change, the log lines, the wait timeout, the scheme colour, routing of the dial, LEFT, RIGHT, BACK and other keys, `enter` and `leave`, the hint, `tick` and the wake time, the wait after a slow or a quick draw for the wake time and the minute |
| `test_list.c` | Selection, the ends, the scroll window for every size up to 30 rows, draw lists, the scroll bar, cut labels, the empty message, coordinates inside the area |
| `test_text.c` | Wrap cases with exact breaks, no line past the width, scrolling, a draw list, coordinates inside the area |
| `test_widgets.c` | Gauge clamps and draw lists, one bar rectangle for a group of value texts, a text wider than its column, key and value rows, sub-tab ends and draw list, the generated icons |
| `test_vfont.c` | Every glyph inside its cell, no two glyphs alike, the crossed box for unknown characters, filled dots at every size, widths, a draw list |

`tests/host/test_pbm2c.py` tests the PBM converter: parsing, bit packing, the generated table, bad input and the files in `assets/`. `make test-host` runs it.

`tests/tools/` tests the content tools with Python's `unittest`, so they need no pytest. `make tools-test` runs them.

| File | Covers |
|---|---|
| `test_check_content.py` | The sample tree passes; a table of broken trees, one per rule of [CONTENT.md](CONTENT.md), each with its message and line; the INI reader against `ini.c`'s rules; the line count against `dx_wrap` |
| `test_tun2notes.py` | The sample tunes stay inside `play_seq`'s limits and last 8 to 30 seconds; note names, lengths, dots, tempo, the repeat, every error; the WAV output |
| `test_mkpcm.py` | Each track's SHA-256, its length, no clipping, the DC level, the loop joint, the command line |

To add one, write `tests/host/test_<name>.c` with `TEST(name) { CHECK(...); }` from RobCo-Core's `rc_test.h`. `make test-host` finds it.

The shell tests are `tests/host/check_pipos_links_test.sh` and `tests/host/sdimage_test.sh`.

### Draw-list tests

A widget test draws into the fake and compares `fake.draws` with the exact list of calls, one per line:

```
rect 0 38 320 16 fg fill
text 12 38 "Spare fuse" bg fg s1
```

Colours print as theme names, so a change of scheme colour does not touch the tests. Keep each list short: a 3-row list, 2 lines of text. Sweeps check a property instead: `fake_points_inside()` says whether every recorded point lies in the widget's area.

When a draw list changes on purpose, run the test, read the "got" side of the failed `CHECK_STR`, check every changed line against the change you meant, and paste it in.

## QEMU tests

`tests/test_deitrix.py` boots the card from `make sdimage` with the Pip-OS kernel. `tests/test_gallery.py` boots the gallery's card from `make gallery`. It uses RobCo-Core's harness and Pip-OS's `tests/pipos_helpers.py`, both from the submodule. Each test waits for `deitrix: ready` on serial, then drives the ROM through the script channel.

| Test | Checks |
|---|---|
| `test_first_screen` | The STAT screen matches `golden/stat.png`, the ROM still runs |
| `test_navigation` | The dial and the LEFT and RIGHT keys give the expected `deitrix: tab=` lines, with wraps both ways; the INV screen matches `golden/inv.png`; keys that do nothing log nothing; the ROM still runs |
| `test_clock_shown` | With a ds1338 RTC at 09:47:00, the header clock matches `golden/clock_0947.png` |
| `test_clock_minute_change` | With the RTC at 09:47:30, the clock first shows 09:47, then 09:48 within 10 s after the minute |
| `test_gallery_screens` | Each gallery screen matches its golden, `golden/gallery_<name>.png`; RIGHT moves through the sub-tabs before it leaves TABS; RIGHT on FONT wraps to LIST; the dial goes back to TABS, which kept its last sub-tab |
| `test_gallery_list_and_text` | The list moves down to its last row and back, with the window at each step; DOWN at the end and UP at the top do not move it; the text scrolls to its end and back; LIST keeps its state across a tab change; goldens at both ends |
| `test_gallery_detail` | SELECT on a list row opens its detail, which matches `golden/gallery_detail.png`; RIGHT stays inside it; LEFT goes back to the list with its selection; LEFT at the top level changes the tab |
| `test_gallery_hold_repeats` | DOWN held for 1.6 s moves the list one row at a time, at least 5 rows, never past the end |

The first two tests boot without an RTC, so the header shows `--:--` and the goldens do not depend on the time. The goldens are 640x480 QEMU screenshots, or crops of them, and must match on every board pixel for pixel.

The gallery logs each list and text move before its redraw. To be sure a frame is on screen, the tests send one more key that changes nothing and wait for its line: the shell draws before it takes the next event.

`test_gallery_hold_repeats` depends on timing. Pip-OS repeats a held key 500 ms after the press, then every 100 ms, so 1.6 s gives the press and about 11 repeats. After a stall the kernel restarts the schedule, so the test asks for 5 rows only.

`test_clock_minute_change` depends on timing. The ROM redraws at most about 1 s after the minute changes. The test allows 10 s more. If it fails on a loaded machine, rerun it alone before you conclude:

```sh
.venv/bin/pytest tests -k minute_change --board raspi2b
.venv/bin/pytest tests -k hold_repeats --board raspi2b
```

## Goldens

After a deliberate change to the screen, write the goldens again on one board, then check every board:

```sh
make sdimage kernel gallery venv
DEITRIX_UPDATE_GOLDEN=1 .venv/bin/pytest tests --board raspi2b
make test-target
```

Look at each changed PNG before you commit it.
