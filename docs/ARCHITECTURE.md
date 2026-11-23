# Architecture

Deitrix is one ROM image. Pip-OS loads it at `0x00400000`, starts it in user mode and calls `pip_main()`. The ROM never returns.

## Source layout

### `src/main.c`

The entry and the main loop. `dx_main()` runs `dx_app_step()` forever. `src/app.h` declares the loop's state, so the host tests can run it one step at a time.

### `src/platform/`

The only code that calls Pip-OS. `platform.c` defines `pip_main()` and wraps the system calls Deitrix uses behind `platform.h`: the scheme colour, the clock, `wait_event`, `log` and the drawing calls. Nothing outside this folder includes `<pipos/pip.h>`. The host tests link `tests/host/fake_platform.c` in its place.

### `src/model/`

Pure data code, with no drawing and no system calls. `nav.c` holds the tab list and the navigation state machine: the step an event asks for, and the move with wrap-around. `clock.c` turns Unix seconds into the minute of the day, its `HH:MM` text and the wait to the next minute.

### `src/ui/`

Drawing helpers and widgets. `frame.c` draws the frame every screen shares: the tab bar with the clock, the rules and the key hint. It also derives the theme: the foreground is the colour Pip-OS reports in `gfx_info` for the card's scheme, and the dim colour is that colour at half brightness.

### `src/screens/`

One file per tab, later. Today `placeholder.c` draws the same body for every tab: its name in large text and "Not built yet."

### `content/`

Files copied to the card root by `make sdimage` and `make bootdir`. Today it holds `PIPBOY/DATA/README.TXT`.

### `tests/`

`tests/host/` holds the C host tests and the fake platform. `tests/*.py` are the QEMU tests, with their screen goldens in `tests/golden/`. See [TESTING.md](TESTING.md).

## The main loop

Each step:

1. Reads the clock. Without one, the minute is "none".
2. Redraws when the tab or the minute differs from what is on screen: clear, frame, body, then one `present`.
3. Logs on serial after the `present`: `deitrix: ready` after the first frame, `deitrix: tab=<NAME>` after a frame for a new tab.
4. Blocks in `wait_event` until the next whole minute by the clock's seconds, or with no timeout when there is no clock.
5. Turns the event into a tab step. A dial event gives its detents. A LEFT or RIGHT press or repeat gives -1 or +1. Other events give nothing.

The clock has whole seconds only, so the wait can end up to 1 s before the minute changes. Then the next step sees the same minute, does not redraw, and waits again, at most 1 s. The time is shown in UTC: Pip-OS has no time zone.

## Planned

These are plans, not code:

- A screen interface with `enter`, `leave`, `event`, `tick` and `draw`, one implementation per tab in `src/screens/`.
- Widgets in `src/ui/`: a list, a text block and a gauge.
- Content files under `/PIPBOY/DATA/` on the card, from `content/`.
- Saves through the Pip-OS save calls.
- The five screens: STAT, INV, DATA, MAP and RADIO.
- A holotape game.
