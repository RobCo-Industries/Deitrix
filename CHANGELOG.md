# Changelog

All notable changes to Deitrix are listed here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Deitrix follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Pip-OS moves to `084590e`, with RobCo-Core `300e5a3` inside it. The sound table generator takes `--bed` for sounds over a loop. The CI workflow pins RobCo-Core `300e5a3`. The docs link the new commits.
- `make boot-sound` renders the log with RobCo-Core's `tools/audio_preview.py`, since Pip-OS removed its `tools/boot_sound.py`. The WAV keeps the same bytes.
- Pip-OS moves to `693e457`, with RobCo-Core `c0bee99` inside it. The library now holds the audio preview tool and adds `loop=1` to the trace of a looped play. Deitrix's radio refills a queue and never starts a looped play, so no test or doc changes. The CI workflow pins RobCo-Core `c0bee99`. The docs link the new commits.
- Pip-OS moves to `5175bbd`, with RobCo-Core `9a11561` inside it: ABI 1.5, the system sound, disk clicks and centred tones. The SDK seals the ROM for ABI 1.5, and STAT's ABOUT page shows `ABI 1.5`. With the system sound off, the default, Deitrix sounds and looks as before.

- Pip-OS moves to `93a07d2`, the commit that builds the `pipos` 1.4.0 SDK package, with RobCo-Core `712db81` inside it. Its `pipos-app.mk` takes `ABI`, writes `flags.json` with `make flags-json` and maps the build paths with `-ffile-prefix-map`. Culvert's tape keeps the same bytes. The CI workflow pins RobCo-Core `712db81` with the same image tag. The docs link the new commits.
- Pip-OS moves to `ca2e054`, with RobCo-Core `cab5f69` inside it. The SDK seals the ROM for ABI 1.4. STAT's ABOUT page shows `ABI 1.4`. The CI workflow pins RobCo-Core `cab5f69` with the same image tag. The docs link the new commits.
- The docs record the first hardware run: on a Pi Zero W 1.1, Deitrix booted, took serial keys and wrote DEITRIX.SAV. `docs/BUILD.md` gives the Mac commands that made the card.
- The docs record a second hardware run on the same card: DEITRIX.SAV loaded at boot, serial keys reached all five tabs, and Culvert started, took keys and quit with status 0.
- `tools/check_content.py --help` prints the usage instead of checking a folder named `--help`. Exit codes stay the same.
- STAT's ABOUT page shows the true uptime past 24.8 days: a new uptime service counts the wraps of `ticks_ms`.
- The profile's gauge type is `dx_profile_gauge_t`, apart from the gauge widget. The widget has a selected, inverse mode, and RADIO's Volume row uses it.
- STAT's ABOUT page logs its device line when it shows and once per uptime minute, not every second.
- The header shows a `!` left of the clock while changes have not reached the card for 12 s, for example on a card that cannot be written.
- Every tab uses one layout grid for its body. Lists start 6 px under the sub-tab row or the header, and status rows sit on one line at the bottom.
- The footer hints follow one style: the same key names, two spaces between groups, movement first, at most 38 characters. No screen hint names the tab change.
- One held-key rule for every tab, in the shell: after a press opens or closes a view or changes the tab, the repeats of that key do nothing until the next press. A held LEFT in INV's detail no longer changes the category and the tab.
- Pip-OS moves to `2d10c2c`, with RobCo-Core `f5bba36` inside it. The kernel mounts the card with the shared boot volume rule.
- Pip-OS moves to `5f891e7`, with RobCo-Core `79cbd82` inside it. The SDK seals the ROM for ABI 1.3. Pip-OS skips the macOS `._*` side files on the card, so `docs/BUILD.md` no longer asks for `dot_clean`.

### Added

- "UI sounds" gains a fourth value, `Full + hum`, offered when `Full` is. A hum service plays the hum while the radio is off and no tape runs: its two parts take turns in the kernel's queue, topped up every 250 ms. Each table then plays as a head, mixed with the hum's first bytes into RAM at start-up, with the rest of the hum's first part queued behind it. The heads take 56 732 bytes of BSS. A station and a tape stop the hum, and it comes back after their last sound. The value sits in the same byte of DEITRIX.SAV. A saved 3 without the midpoint flag plays the tones and shows `On`. `make sfx-table` passes `--bed hum`, so the header also gives each head's and rest's length and CRC-32. `make sfx-preview` saves `Full + hum`. The ROM grows from 138 272 to 225 880 bytes, mostly the hum's tables. Host tests in `test_hum.c` and `test_saves.c`, QEMU tests in `test_sfx_hum.py`.
- With "UI sounds" on `Full`, SELECT on a station plays `tuner` when it starts and when it stops. The station starts from a new tuning state of the radio service, once the tuner has ended: at least 100 ms after it and when the audio status reads idle, or at 300 ms at most. A failed delayed start logs `deitrix: radio cannot play`, as an immediate one does. A holotape plays `tape_start` before it starts and `tape_stop` when it returns. TAPES waits up to 500 ms for `tape_start` to end, since Pip-OS stops a ROM's sound at a launch. The kernel's clock bounds the wait, so a burst of keys cannot cut it short. These keys play no `select` as well: the new result flag `DX_UI_SOUNDED` tells the shell. `On` and `Off` keep every tone. The ROM grows from 101 872 to 138 144 bytes with the three tables. Host tests in `test_sfx.c`, `test_data.c` and `test_radio_tab.c`, QEMU tests in `test_sfx_tape_tuner.py`. The `make sfx-preview` session now checks the three sounds.
- `make sfx-preview` drives a session with "UI sounds" on Full in QEMU and writes `build/sfx_preview.wav`: the boot, the five modes, a station, rows, a sub-tab, SELECT, BACK and a tape. It fails on a missing log, a warning of the preview tool or a silent file.
- With "UI sounds" on `Full`, a tab change plays `mode`, a sub-tab move plays `tab` and BACK plays `highlight`. An INV category change is a sub-tab move, so it plays `tab`. `dx_subtabs_event()` sets the new result flag `DX_UI_TAB_MOVED` when the sub-tab changes. One key plays one sound, in this order: mode, tab, row, then the move tone. Deny keeps its 180 Hz tone. `On` and `Off` keep every tone. Host tests in `test_ui.c` and `test_sfx.c`, QEMU tests in `test_sfx.py`.
- "UI sounds" gains a third value, `Full`. A list row move then plays `scroll` and a select plays `select`, two 8-bit tables at 37500 Hz, in place of their tones. Only a list row move plays `scroll`: the list sets the result flag `DX_UI_ROW_MOVED` when the highlighted row changes. A count change, a volume step, a text scroll and a new map place keep the move tone. RADIO offers `Full` only when the kernel reports ABI 1.5, an audio driver and an output that rests at its midpoint. Otherwise a saved `Full` plays the tones. The value sits in the same byte of DEITRIX.SAV. All sounds are original synthesis from the parameters in `assets/sfx/sfx_models.json`. RobCo-Core's `tools/sfx_table.py` writes the committed `src/services/sfx_table.c` and `.h`, and every build stops on a stale table. `make sfx-table` writes them again. The ROM grows from 79 880 to 87 568 bytes: the linker keeps only the two tables that play. Host tests in `test_sfx.c` and `test_saves.c`, tool tests in `test_sfx_table.py`, QEMU tests in `test_sfx.py`.
- The weep: a 140 ms rising chirp at 37500 Hz, played once right after the first frame. It needs ABI 1.5, an audio driver and an output that rests at its midpoint, which Pip-OS gives with `[audio] system_sound` on. It stays silent with UI sounds off or while the radio plays. `dx_plat_sample_rate()` wraps `sample_rate`. `tools/mkweep.py` writes the committed table from `tools/weep_params.json`, and `make weep-table-check` stops the build when it is stale. Host tests in `test_sfx.c`, tool tests in `test_mkweep.py`, QEMU tests in `test_weep.py`.
- `make boot-sound` boots the card with the system sound on and renders the whole boot, the weep included, into `build/boot_sound.wav` with Pip-OS's preview tool.

- `holotapes/culvert/robco.toml`, Culvert's manifest for RobCo Studio. Culvert's Makefile states `ABI := 1.4`, the ABI its tape already carried, so both files state the same ABI. The sources keep the Makefile's order, and the tape keeps the same bytes.
- The holotape slot of ABI 1.4. A tape slot service reads `hw_flags` and, with a slot, asks `holo_eject_event` after each `PIP_EV_TAPE` event, each key and each launch. TAPES reads the list again after a change and keeps the selection on its tape, or on its row when the tape left. A slot tape shows `SLOT GAME` or `SLOT UTIL`. The footer shows a notice for 3 s when a card goes in, comes out or cannot be read, on any tab, and the keys keep working. A tape whose card is pulled while it runs comes back with "Tape pulled while running.". With no slot, or on a Pip-OS before 1.4, nothing changes.
- `make holotest-kernel` builds Pip-OS's slot test kernel through its Makefile, and `tests/test_slot.py` drives its RAM card backend in QEMU. Host tests in `test_notice.c` and `test_data.c`. Nothing here ran on a board.
- Culvert, a pipe-turning puzzle holotape with generated levels, built by `make holotapes` and shipped on the card as `CULVERT.HTP`. DATA's TAPES list starts it.
- The STAT tab: the owner and gauges, the attributes, and the device data with the uptime, refreshed each second.
- The INV tab: one sub-tab per category, a list of its items with a summary row, and a detail view where UP and DOWN change the count and SELECT marks the item.
- The DATA tab: the notes, a holotape launcher, and the clock with a time setter.
- The MAP tab: the local and world maps with their places, and a detail per place.
- The RADIO tab: the stations with the one on air marked, what plays now, the volume and the UI sounds. SELECT starts or stops a station.
- A data model in `src/model/` that loads PROFILE.INI, ITEMS.INI, the notes, MAP.INI, RADIO.INI and the tunes as `docs/CONTENT.md` says, with fixed tables and no allocation. One serial line per file loaded.
- DEITRIX.SAV and INV.SAV: the settings and the item overlay, read at start and written by an autosave service.
- Services in the app shell: a table of background jobs with their own wake times. The radio service plays tunes and streams PCM tracks on any tab.
- UI tones stay silent while the radio plays, and when the settings turn them off.
- The platform layer reads files, lists folders, reads and writes saves, plays sequences and samples, and reports system information. It wraps libpip's INI parser and CRC-32.
- Host tests for every loader, the saves, the services and the content loader, with fixtures generated from `tools/tun2notes.py` and the checker's test table. QEMU tests for the load log, a save across a reboot, a broken file, a card without data and the radio.
- Pip-OS as the only submodule, at `4178b7f`, with RobCo-Core `222f4e4` inside it.
- `build/ROM1.ROM` built with the Pip-OS SDK: a frame with five tabs, a UTC clock and a key hint. The dial, LEFT and RIGHT change tabs.
- `make sdimage`, `run` and `bootdir` through Pip-OS's Makefile, with the `content/` tree on the card.
- Host unit tests, QEMU tests on four boards with screen goldens, and a card check.
- `make check-pins` and `tools/check_pipos_links.sh`.
- CI through RobCo-Core's reusable workflow.
- README, build, testing, architecture and contributing docs.
- `docs/USER_MANUAL.md` for the person who wears the device, `docs/ASSETS.md` for every asset and its licence, and a build guide for Mac and Linux in `docs/BUILD.md`.
- A UI framework: a drawing table in the platform layer, a theme with dim and faint shades, UI events, a screen interface and an app shell that routes keys to the active screen. Each tab is a screen in `src/screens/`.
- Widgets: a list, a text block, a gauge, key and value rows, sub-tabs, 1-bit icons and a stroke font for large text.
- `tools/pbm2c.py`, which turns the icons in `assets/` into C at build time.
- Short UI tones for moves, selections and refused keys.
- `docs/CONTENT.md`: the formats of PROFILE.INI, ITEMS.INI, the notes, MAP.INI, RADIO.INI, `.TUN` tunes, `.PCM` tracks and the two saves, with every limit.
- Sample content in `content/PIPBOY/DATA/`: a profile, 14 items, 4 notes, two maps, three tunes and five stations. `content/PIPBOY/DATA/README.TXT` is gone.
- `tools/check_content.py`, run by `make check-content`; `tools/tun2notes.py`; `tools/mkpcm.py`, which generates the two PCM tracks at build time. `make card-tree` puts them with `content/` in `build/card/`, the tree that goes to the card.
- Tests for the content tools in `tests/tools/`, run by `make tools-test`.
- `make gallery`: a test-only ROM that shows every widget, with QEMU goldens and scripted runs.
