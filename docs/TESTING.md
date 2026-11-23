# Testing

## Layers

| Layer | Command | What it covers |
|---|---|---|
| Host unit tests | `make test-host` | Pure code in `src/model/`, `src/ui/`, `src/screens/`, `src/services/`, the content loader and the app shell, against a fake platform; `tools/pbm2c.py` |
| Pin checks | `make check-pins`, part of `test-host` | The CI workflow pins and the commit links in the docs |
| Environment guard | `tests/test_env_hygiene.py`, part of `test-target` | No test or tool copies `os.environ` |
| Content check | `make check-content`, part of `test-host` | `build/card/` follows [CONTENT.md](CONTENT.md) |
| Tool tests | `make tools-test`, part of `test-host` | `tools/check_content.py`, `tools/tun2notes.py` and `tools/mkpcm.py` |
| Card check | `make sdimage-test`, part of `test-host` | The card holds the ROM, the kernel, `PIPOS.CFG` and every file of `build/card/` with the same bytes; `build/card/` holds every file of `content/` and both PCM tracks |
| QEMU tests | `make test-target` | The ROM and the widget gallery on Pip-OS on `raspi0`, `raspi1ap`, `raspi2b` and `raspi3b` |

`make test` runs `test-host`, then `test-target`. `BOARD=raspi0` limits the QEMU tests to one board.

## Host tests

Each `tests/host/test_<name>.c` builds into one program with RobCo-Core's `mk/host.mk`: `-Werror`, AddressSanitizer and UndefinedBehaviorSanitizer when the compiler has them. Every program links all of `src/` except `src/platform/`, the generated `build/gen/icons.c`, `tests/host/fake_platform.c` and RobCo-Core's `src/lib/`, which holds the `ini.c` and `crc32.c` that libpip wraps. The fake records draw calls, log lines, tones and audio calls, and plays back scripted events, clock readings and ticks. It can also move its clock and tick counter forward at each `present`, to play a slow draw. It holds an in-memory card, a save store that can fail on purpose, and one audio voice that plays against its tick counter: a queued sample takes over when the one before ends.

`tests/host/gen_expect.py` writes `build/gen/expect.h` before the host tests build. It holds the notes `tools/tun2notes.py` makes of each sample tune and of about 50 edge cases, or the line of the error. It also holds every broken tree of `tests/tools/test_check_content.py`, as the files each one changes.

### Leaks

CI runs LeakSanitizer with GCC's AddressSanitizer; Apple clang on macOS does not. So the tests allocate only through `fake_alloc()` and `fake_free()`, which count live blocks, and the fake owns every card file it loads or is handed with `fake_add_owned()`. Each test program ends with `fake_reset()` and fails, with the count, when a block is still live; `make check-format` fails on a direct `malloc`, `calloc`, `realloc`, `strdup` or `free` in `tests/host/` outside `fake_platform.c`.

### Formatted writes

CI builds the host tests with GCC and `-Werror`. GCC's `-Wformat-truncation` fails a `snprintf` whose output may not fit and whose result is unused. Apple clang does not run that analysis, so such a call passes locally and fails in CI.

The rule: in `tests/host/`, format with `tfmt()` or `tvfmt()` from `tests/host/tfmt.h`. They work like `snprintf` but stop the test run with a message when the output does not fit. `src/` calls no libc formatter. `make check-format`, part of `make test-host`, fails on a direct `sprintf`, `snprintf`, `vsnprintf`, `vsprintf`, `strcpy`, `strcat`, `strncpy` or `strncat` anywhere else.

For other GCC-only warnings, CI is the check. If a GCC is installed, run `make CI=1 test-host HOST_CC=gcc-15` (use your GCC's name).

| File | Covers |
|---|---|
| `test_nav.c` | Next and previous, wrap, several detents both ways, extreme deltas, any tab count |
| `test_clock.c` | `00:00`, `23:59`, `--:--`, the minute of the day, the wait to the next minute |
| `test_ui.c` | The theme shades, event translation with button 6 as BACK, `dx_ui_is_back()`, the sound for each outcome, the tones, number text, cut text; the held-key latch |
| `test_frame.c` | The header and footer draw lists, the active-tab block, the tab pad with many tabs, every label left of the clock for 1 to 8 tabs of 1 to 5 letters, the cut labels, the refused tables, the ROM's screen table |
| `test_hints.c` | Every footer hint of every state on three cards: the length, the key names, the two-space separator and the order; the walk reaches every hint of the table in `docs/ARCHITECTURE.md`; the rules refuse a bad hint |
| `test_app.c` | The refused tab tables, one `present` per redraw, the clip around the body, no redraw without a change, the log lines, the wait timeout, the scheme colour, routing of the dial, LEFT, RIGHT, BACK and other keys, `enter` and `leave`, the hint, `tick` and the wake time, the wait after a slow or a quick draw for the wake time and the minute; services: their ticks, the earliest wake, a tick after each event, the tick wrap, a bad table, the start tab |
| `test_list.c` | Selection, the ends, the scroll window for every size up to 30 rows, draw lists, the scroll bar, cut labels, the empty message, coordinates inside the area |
| `test_text.c` | Wrap cases with exact breaks, no line past the width, scrolling, a draw list, coordinates inside the area |
| `test_widgets.c` | Gauge clamps and draw lists, one bar rectangle for a group of value texts, a text wider than its column, key and value rows, sub-tab ends and draw list, the generated icons |
| `test_vfont.c` | Every glyph inside its cell, no two glyphs alike, the crossed box for unknown characters, filled dots at every size, widths, a draw list |
| `test_profile.c` | One case per PROFILE.INI rule: cuts, clamps, dropped gauges and attributes, the last key wins, ignored files; the sample |
| `test_items.c` | Categories and items, every drop and default, the 64-item limit, the overlay's ranges and change count, the totals and whether a category has a weight; the sample |
| `test_notes.c` | Name order, the first 32, the filter, titles, the body's line ends and control bytes |
| `test_map.c` | Maps, boxes, lines and places with every drop; the fit for wide, tall, one-unit and one-pixel boxes, a sweep, the degenerate cases; the sample |
| `test_radio.c` | Stations and their drops, availability, and the tune parser against `tun2notes.py` note for note and error line for error line |
| `test_saves.c` | Both layouts pinned as hex, round trips of random states with a fixed seed, every bit flipped, every wrong size, fields out of range, record rules |
| `test_services.c` | The radio: tune replay, PCM chunks with busy answers, the loop join, an underrun, a track played once, stop in every state, a missing file, a read error, volume, the tick wrap. The autosave: the quiet time, the 10 s bound, flush, failed writes and retries, the tick wrap. After a give-up: no more writes on ticks, one attempt per flush, the changes stay unsaved until a write works, a new change retries again, one failing save leaves the other alone, the retries across the tick wrap |
| `test_stat_fmt.c` | STAT's text helpers: the uptime, the date, the versions, the SoC and the RAM |
| `test_stat.c` | The STAT tab: the name's font size, each sub-tab with a full, a minimal, a missing and a broken profile, the ATTRS scroll, the keys STATUS ignores, the sub-tab kept across tabs, the ABOUT tick, a day of uptime and the tick wrap, every view inside the body, no block live at the end; a held LEFT changes the tab once |
| `test_inv_fmt.c` | INV's text helpers: weights in grams and kilograms, counts, the item of each row, the summary row, at the limits |
| `test_inv.c` | The INV tab: no ITEMS.INI, a broken file, no category, the first view's draw list, the categories and an empty one, list moves and ends, the detail, the count at 0 and 999, the mark and the summary, routing through the shell and `leave`, the longest names and most items inside the body; a held SELECT opens once and a held LEFT or BACK closes once through the shell |
| `test_data_time.c` | The DATA clock's date helpers: leap years, known dates and their weekdays, every day from 2000 to 2099 against a reference, each setter field's wrap, the day clamp, the texts |
| `test_data.c` | The DATA tab: no notes and a folder error, a note opened and closed, 32 notes with the scroll and the kept selection, a cut and an unreadable note, the note's draw list; no tapes and a list error, the kinds, titles and icon, the launch order and every result line, the list after a launch, the radio off with no failure on RADIO afterwards, a failed save that asks for a second press, the same after the autosave gave up, a refused data tape, 32 tapes; the clock page and its tick, no clock, the setter's start, every key in every field, its wraps and clamps, a store that works, fails or is refused; every hint fits the footer |
| `test_map_view.c` | MAP's pure helpers: the pane fit for the sample maps and at the extremes, a pane too small, a line in pixels, the place cycling, the marker squares, the position text |
| `test_map_screen.c` | The MAP tab through the app shell: the first view, the markers, the place cycle both ways, the detail and back, the sub-tabs and the tabs beside them, the dial from the detail, one map with one place, no places, places without lines, no file, a rejected file, a file without maps, every draw inside the body, the hints, no tick; a held SELECT opens once and a held LEFT or BACK closes once |
| `test_radio_tab.c` | The RADIO tab: the first view's draw list, start, stop and a switch with the audio calls, the station's kind under what plays, the block through a track's wrap and end, a station with nothing to play, the volume to both limits, a held SELECT that starts and stops once, the UI sounds with held keys, LEFT and RIGHT on a station and on a setting, the cursor at the last station, no file, a broken file, every draw inside the body; a held LEFT changes the tab once on a station and steps the volume on each repeat |
| `test_content.c` | The sample card's load log, line for line; a card without data; read errors; the saves at start; every broken tree of the checker's table, with the log lines it changes, in `test_content_expect.inc` |

`tests/host/test_pbm2c.py` tests the PBM converter: parsing, bit packing, the generated table, bad input and the files in `assets/`. `make test-host` runs it.

`tests/tools/` tests the content tools with Python's `unittest`, so they need no pytest. `make tools-test` runs them.

| File | Covers |
|---|---|
| `test_check_content.py` | The sample tree passes; a table of broken trees, one per rule of [CONTENT.md](CONTENT.md), each with its message and line; the INI reader against `ini.c`'s rules; the line count against `dx_wrap` |
| `test_tun2notes.py` | The sample tunes stay inside `play_seq`'s limits and last 8 to 30 seconds; note names, lengths, dots, tempo, the repeat, every error; the WAV output |
| `test_mkpcm.py` | Each track's SHA-256, its length, no clipping, the DC level, the loop joint, the command line |

To add one, write `tests/host/test_<name>.c` with `TEST(name) { CHECK(...); }` from RobCo-Core's `rc_test.h`. `make test-host` finds it.

The shell tests are `tests/host/check_pins_test.sh` and `tests/host/sdimage_test.sh`. `check_pins_test.sh` runs `make check-pins` in a temporary copy of the docs, with a fake `git` that always fails first on `PATH`. The copy passes. A stale Pip-OS link and a stale RobCo-Core link each fail, with the file, the line and the repository.

A test that starts a subprocess builds its environment with RobCo-Core's `minimal_env()`, from `harness/env.py`. It never passes a copy of `os.environ`: pytest prints the locals of a failing test, secrets included. `tests/test_env_hygiene.py` scans every `.py` and `.sh` file under `tests/` and `tools/` and fails on any copy of `os.environ`.

### Draw-list tests

A widget test draws into the fake and compares `fake.draws` with the exact list of calls, one per line:

```
rect 0 38 320 16 fg fill
text 12 38 "Spare fuse" bg fg s1
```

Colours print as theme names, so a change of scheme colour does not touch the tests. Keep each list short: a 3-row list, 2 lines of text. Sweeps check a property instead: `fake_points_inside()` says whether every recorded point lies in the widget's area.

When a draw list changes on purpose, run the test, read the "got" side of the failed `CHECK_STR`, check every changed line against the change you meant, and paste it in.

## QEMU tests

`tests/test_deitrix.py`, `tests/test_model.py`, `tests/test_stat.py`, `tests/test_inv.py`, `tests/test_data.py`, `tests/test_map.py` and `tests/test_radio_tab.py` boot the card from `make sdimage` with the Pip-OS kernel. `tests/test_gallery.py` boots the gallery's card from `make gallery`. They use Pip-OS's `pipos_testing` package, from `external/pip-os/sdk/testing/`, as its [Testing a ROM project](https://github.com/RobCo-Industries/Pip-OS/blob/2d10c2c207e17dc8df4e9e8f3c50d60a3dff0bf3/docs/TESTING.md#testing-a-rom-project) shows. `tests/conftest.py` puts that folder on `sys.path` and loads `pipos_testing.plugin`, which brings RobCo-Core's harness. The tests use its helpers:

- `booted(..., wait="jump")` boots the card. Deitrix never exits, so the kernel prints `ev=jump` but never `ev=ready`. Each test then waits for `deitrix: ready` on serial.
- `script()` sends one script-channel command and checks it answered `ok`. `press()` presses and releases a button.
- `serial_line()`, from `tests/conftest.py`, builds the regex for a serial log line. It matches only whole lines: the serial buffer can end in the middle of one, and a cut line must not be parsed. `tests/test_serial_line.py` checks it without QEMU.
- `check_golden(..., update_var="DEITRIX_UPDATE_GOLDEN")` compares a screenshot with its golden.

| Test | Checks |
|---|---|
| `test_first_screen` | The STAT screen matches `golden/stat.png`, the ROM still runs |
| `test_navigation` | The dial and the LEFT and RIGHT keys give the expected `deitrix: tab=` lines, with wraps both ways; the INV screen matches `golden/inv.png`; keys that do nothing log nothing; the ROM still runs |
| `test_clock_shown` | With a ds1338 RTC at 09:47:00, the header clock matches `golden/clock_0947.png` |
| `test_clock_minute_change` | With the RTC at 09:47:30, the clock first shows 09:47, then 09:48 within 10 s after the minute |
| `test_load_log` | One `deitrix: load` line per file of the sample card, with its counts, and both saves missing |
| `test_setting_survives_a_reboot` | SELECT on RADIO starts the first tune, whose first trace tone is 392 Hz; the autosave writes DEITRIX.SAV; a second boot of the same card loads it and opens on RADIO |
| `test_one_broken_file` | A MAP.INI with version 2 is ignored and every other file loads |
| `test_no_data_folder` | Without `/PIPBOY/DATA` every file is missing, every tab draws, SELECT on RADIO does nothing |
| `test_radio_streams_pcm` | Station 4 streams: one `op=pcm`, then `op=pcm_next` of 8000 bytes with no `op=pcm_end`; a tab change does not stop it and plays no tone; no underrun; turning the radio off stops the sound |
| `test_radio_end_redraws` | A sixth station plays a 2 s track once; when the trace shows `op=pcm_end`, RADIO shows off again within 1.25 s with no key |
| `test_stat_walk` | Each STAT view matches its golden, `golden/stat*.png`; STATUS ignores UP, DOWN, SELECT and Back; ATTRS moves to its last row and back; ABOUT logs the device rows and its uptime grows by 2 or 3 s in two ticks; RIGHT on ABOUT moves to INV and LEFT comes back to ABOUT; the tick stops on another sub-tab; LEFT on STATUS wraps to RADIO |
| `test_stat_no_profile` | Without PROFILE.INI, STATUS and ATTRS match `golden/stat_empty.png` and `golden/stat_empty_attrs.png`; ABOUT still shows the device |
| `test_inv_views` | Each INV view matches its golden, `golden/inv*.png`; RIGHT and LEFT change the category, which keeps its row; UP at the top logs an end; the detail counts and marks; RIGHT in the detail stays, LEFT and Back go back; the dial leaves from the detail and the return shows the list; LEFT at the first category moves to STAT |
| `test_inv_count_refused_at_zero` | DOWN at a count of 0 is refused; UP counts again |
| `test_inv_survives_a_reboot` | A count and a mark reach INV.SAV through the autosave; the next boot of the same card opens on INV and shows them |
| `test_inv_no_items_file` | Without ITEMS.INI the tab matches `golden/inv_empty.png` and SELECT and DOWN do nothing |
| `test_inv_empty_category` | A category without items matches `golden/inv_empty_cat.png`; SELECT does nothing; RIGHT at the last category moves to DATA |
| `test_inv_hold_counts_up` | UP held for 1.6 s in the detail adds one per event, at least 5 |
| `test_inv_held_left_closes_the_detail_once` | LEFT held for 1.5 s in FOOD's detail closes it once; the category and the tab stay |
| `test_data_notes` | NOTES and an open note match `golden/data_notes.png` and `golden/data_note.png`; UP at the top and Back do nothing; DOWN stops at the last note; RIGHT, UP and DOWN stay in the note; LEFT and Back go back with the same note selected; the dial leaves from a note and the return shows the list; LEFT on NOTES moves to INV |
| `test_data_no_notes` | Without notes NOTES matches `golden/data_notes_empty.png` and SELECT opens nothing |
| `test_data_no_tapes` | Without holotapes TAPES matches `golden/data_tapes_empty.png` and SELECT launches nothing |
| `test_data_tapes` | With both test tapes TAPES matches `golden/data_tapes.png`; the radio stops by the time LINETEST ends; LINETEST ends with status 42 and the result matches `golden/data_tapes_status.png`; the radio stays off; FAULT is stopped by the kernel, the launch returns -14 and the result matches `golden/data_tapes_fault.png`; the result line goes when the selection moves; back on RADIO the page shows "Radio off." and no failure; SELECT on station 1 starts it again and a second SELECT stops it |
| `test_data_tapes_failed_save` | A file named `SAVE` in `/PIPBOY` makes every save write fail; the first SELECT on FAULT shows "Save failed. SELECT again runs it." and starts nothing; the second SELECT runs it |
| `test_data_tapes_save_gave_up` | The same blocked card; the test waits for `deitrix: save gave up`, at most `DX_SAVE_MAX_MS` plus the retries plus a 10 s allowance; the first SELECT still refuses and starts nothing; the second SELECT runs FAULT |
| `test_data_clock_without_rtc` | Without an RTC CLOCK matches `golden/data_clock_none.png`; the setter starts at 2026-01-01 00:00; LEFT on the first field and Back cancel; the setter matches `golden/data_setter.png`; RIGHT on the last field stays; SELECT stores 2026-02-01 07:00 and the page then ticks each second |
| `test_data_clock_with_rtc` | With a ds1338 RTC at 2026-11-23 09:30 the page shows that time; the setter starts from it; one year up and SELECT store the time, and the chip holds it |
| `test_data_held_select_runs_a_tape_once` | SELECT held for 1.5 s on TAPES starts FAULT once, with one result |
| `test_data_held_select_opens_the_setter_once` | SELECT held for 1.5 s on CLOCK opens the setter once and stores nothing |
| `test_map_walk` | Each MAP view matches its golden, `golden/map_local.png`, `golden/map_local_place.png`, `golden/map_detail.png` and `golden/map_world.png`; DOWN and UP cycle the places with a wrap; the detail ignores RIGHT, UP, DOWN and SELECT; Back and LEFT return with the place kept; the sub-tab and places stay across a visit to RADIO; the dial leaves the detail |
| `test_map_no_file` | Without MAP.INI the tab matches `golden/map_empty.png`, the keys do nothing and RIGHT changes the tab |
| `test_map_no_places` | A map without places matches `golden/map_no_places.png` |
| `test_map_clip` | A map whose lines and places reach its box's edges draws inside the pane |
| `test_map_redraw_cost` | The largest map the format allows: each redraw of the map view logs under 100 ms |
| `test_radio_views` | RADIO matches `golden/radio.png`, `golden/radio_playing.png` and `golden/radio_volume.png`; LEFT on a station changes the tab; a tune's first trace tone is 392 Hz; a switch from a tune to a PCM station gives one `op=pcm` and no tone after it; SELECT on the station on air stops it with `op=stop`; a switch from PCM to a tune gives `op=stop` before the first tone; LEFT, RIGHT and SELECT on the settings stay on the tab |
| `test_radio_volume_limits` | RIGHT up to 100 and LEFT down to 0, one line per step, then a refused line at each end |
| `test_radio_survives_a_reboot` | A station, the volume and UI sounds reach DEITRIX.SAV; the next boot opens on RADIO with the station picked and not playing, and matches `golden/radio_restored.png` |
| `test_radio_plays_across_the_tabs` | Station 5 streams through a walk over every tab: one `op=pcm`, no `op=stop`, no `op=pcm_end`, no tone, no underrun |
| `test_radio_no_file` | Without RADIO.INI the tab matches `golden/radio_empty.png`; SELECT does nothing and the settings still work |
| `test_radio_station_missing` | A station whose tune is gone: SELECT logs `cannot play`, the tab matches `golden/radio_nothing.png`, and only UI tones follow |
| `test_radio_long_run` | On `raspi0` and `raspi2b`, station 4 plays for 60 s: at least 55 `op=pcm_next`, no `op=pcm_end`, no `op=stop`, no underrun |
| `test_gallery_screens` | Each gallery screen matches its golden, `golden/gallery_<name>.png`; RIGHT moves through the sub-tabs before it leaves TABS; RIGHT on FONT wraps to LIST; the dial goes back to TABS, which kept its last sub-tab |
| `test_gallery_list_and_text` | The list moves down to its last row and back, with the window at each step; DOWN at the end and UP at the top do not move it; the text scrolls to its end and back; LIST keeps its state across a tab change; goldens at both ends |
| `test_gallery_detail` | SELECT on a list row opens its detail, which matches `golden/gallery_detail.png`; RIGHT stays inside it; LEFT goes back to the list with its selection; LEFT at the top level changes the tab |
| `test_gallery_back_button` | The Back button, from `key back 1` and `key back 0` and from Backspace on the console, leaves the detail for the list like LEFT; at the top level Back does not change the tab |
| `test_gallery_hold_repeats` | DOWN held for 1.6 s moves the list one row at a time, at least 5 rows, never past the end |

`test_data_tapes` builds two test holotapes from `tests/tapes/` with the Pip-OS SDK into `build/tapes/` and copies them to `/PIPBOY/HOLO/` on the test's card:

| Tape | Does |
|---|---|
| `tests/tapes/linetest/` | LINETEST.HTP, with an icon. It draws a frame, a line and its name, logs `linetest: ready`, waits for SELECT and exits with status 42 |
| `tests/tapes/fault/` | FAULT.HTP, a utility. It draws a filled box, logs `faulttest: fault now` and runs an undefined instruction, so Pip-OS stops it |

The first two tests boot without an RTC, so the header shows `--:--` and the goldens do not depend on the time. The goldens are 640x480 QEMU screenshots, or crops of them, and must match on every board pixel for pixel.

The gallery logs each list and text move before its redraw. To be sure a frame is on screen, the tests send one more key that changes nothing and wait for its line: the shell draws before it takes the next event.

`test_gallery_hold_repeats` depends on timing. Pip-OS repeats a held key 500 ms after the press, then every 100 ms, so 1.6 s gives the press and about 11 repeats. After a stall the kernel restarts the schedule, so the test asks for 5 rows only.

`test_inv_hold_counts_up` holds UP the same way and asks for 5 steps. The two `test_data_held_select` tests hold SELECT for 1.5 s and check that it acts once.

Every test in `test_radio_tab.py` first writes `volume = 80` into the card's `PIPBOY/PIPOS.CFG`. Pip-OS starts at that level and the ROM takes it as its volume default. The goldens show the volume, so they do not depend on the sample config of Pip-OS.

The audio trace may show no `op=stop` when a tune gives way to a PCM station. The ROM calls `audio_stop`, then `sample`. RobCo-Core's `audio_stop()` and `audio_play_pcm()` both write the one pending request, and the audio tick reads it later. When both calls land before the same tick, the tick sees only the PCM request. It stops the sequence without a trace line and prints `op=pcm`. So the test checks that no tone follows `op=pcm`. A stop from PCM always prints `op=stop`: the tick traces the cut stream.

`test_radio_streams_pcm` depends on timing: the ROM must queue each chunk within the second the one before plays. It waits up to 30 s for two chunks. `test_setting_survives_a_reboot` waits up to 20 s for the save, written 2 s after the change.

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
