# Architecture

Deitrix is one ROM image. Pip-OS loads it at `0x00400000`, starts it in user mode and calls `pip_main()`. The ROM never returns.

## Source layout

### `src/main.c`

The entry and the main loop. `dx_main()` runs `dx_app_step()` forever. `src/app.h` declares the loop's state, so the host tests can run it one step at a time.

### `src/platform/`

The only code that calls Pip-OS. `platform.c` defines `pip_main()` and wraps the system calls Deitrix uses behind `platform.h`: the scheme colour, the clock, the tick counter, `wait_event`, `log`, `tone` and the drawing table. Nothing outside this folder includes `<pipos/pip.h>`. The host tests link `tests/host/fake_platform.c` in its place.

### `src/model/`

Pure data code, with no drawing and no system calls. `nav.c` moves between tabs with wrap-around. `clock.c` turns Unix seconds into the minute of the day, its `HH:MM` text and the wait to the next minute.

### `src/ui/`

The UI framework: the core types in `ui.h`, the frame every tab shares, and the widgets. See [The UI framework](#the-ui-framework).

### `src/screens/`

One file per tab, each defining one `dx_screen_t`: `stat.c`, `inv.c`, `data.c`, `map.c` and `radio.c`. `table.c` lists them in tab order. Today each draws the shared placeholder from `placeholder.c`: the tab's name in large text and "Not built yet."

### `assets/`

The icons, as plain PBM files. `tools/pbm2c.py` turns them into `build/gen/icons.c` at build time. That file is not committed.

### `content/`

Files copied to the card root by `make sdimage` and `make bootdir`. Today it holds `PIPBOY/DATA/README.TXT`.

### `tests/`

`tests/host/` holds the C host tests, the fake platform and the `pbm2c.py` test. `tests/gallery/` holds the widget gallery's screens. `tests/*.py` are the QEMU tests, with their screen goldens in `tests/golden/`. See [TESTING.md](TESTING.md).

## The UI framework

### The platform seam

`platform.h` declares `dx_gfx_t`, a table of drawing functions: `clear`, `line`, `rect`, `text`, `polyline`, `arc`, `blit1`, `clip` and `present`. Each call takes its own colour, so no drawing state carries over between calls. `platform.c` maps each one to the Pip-OS call that does it: it sets the colour with `set_color`, then draws. `blit1` fills a background rectangle first when one is given, because `blit_1bpp` draws set bits only. Arguments outside the kernel's limits draw nothing. `DX_COLOR_NONE` is a transparent text background.

The host fake gives a recorder instead. Each call becomes one line of a draw list, for example `text 8 4 "STAT" fg none s1`. Colours that match the theme print as `fg`, `dim`, `faint` or `bg`.

### The theme

`dx_theme_from_fg()` builds the theme from the scheme colour Pip-OS reports in `gfx_info`. `dim` is that colour at 1/2 per channel and `faint` at 1/4. `bg` is black. No other code names a colour.

### Events and routing

`dx_ui_translate()` turns a Pip-OS event into a `dx_ui_event_t`. A press gives an event and a repeat gives one with `repeat` set. A release gives none. A dial turn gives `DX_UI_DIAL` with its detents in `delta`. The dial push is SELECT.

`DX_UI_BACK` exists, but no Pip-OS input produces it yet: the kernel keeps its Back key for itself. A screen can handle it today and will get it once Pip-OS passes it on.

The app shell routes each event:

1. A dial turn changes the tab by its detents, with wrap-around. The screen does not see it.
2. Every other event goes to the active screen. It returns `DX_UI_IGNORED`, or `DX_UI_HANDLED`, optionally with `DX_UI_DIRTY` to ask for a redraw.
3. LEFT or RIGHT that the screen ignores moves to the previous or next tab.
4. Anything else the screen ignores does nothing.

A tab change calls `leave` on the old screen and `enter` on the new one, then redraws.

### Sounds

`dx_ui_sound()` plays a short tone through the platform: MOVE, SELECT, BACK or DENY. `dx_ui_sound_for()` picks it: MOVE for a tab change or a handled key that changed the screen, SELECT or BACK when the screen handled one, and DENY for UP, DOWN, SELECT or BACK that the screen ignored on a fresh press. Repeats never deny. `DX_UI_SOUND` in `ui.h` turns every tone off when set to 0.

### Layout

`ui.h` holds the bands of the screen. The skeleton's positions stay, so its goldens still match:

| Band | Rows |
|---|---|
| Header: the tab bar and the clock, with a rule at row 21 | 0 to 21 |
| Body | 22 to 214 |
| Footer: a rule at row 215 and the hint at row 220 | 215 to 239 |

A row of text is 16 px, the font cell. At scale 1 the screen holds 40 columns of 8 px.

The tab labels must end 8 px before the clock, at x 268. The header puts 8 px on each side of a label. When the labels do not fit, it narrows that space, down to 2 px. When they still do not fit, it cuts every label to 4 letters, then to 3. `dx_frame_tab_cut()` gives the letters it keeps. The limits are 8 tabs and 5 letters per name, and with them 3 letters always fit:

| Tabs of 5 letters | Letters shown |
|---|---|
| 1 to 6 | 5 |
| 7 | 4 |
| 8 | 3 |

`dx_app_init()` refuses a table with no screens, more than 8, or a name that is empty or longer than 5 letters. It logs `deitrix: refused: ...` and the ROM stops there.

### The screen interface

A screen is a `dx_screen_t`: a tab label of 1 to 5 letters, six callbacks and a state pointer. Any callback may be NULL.

- `enter` and `leave` run when the tab becomes active and when it stops.
- `event` gets each routed event.
- `tick` runs when the screen's wake time passes. It returns the next wake time in ticks, or 0 for none, and may set `DX_UI_DIRTY`. The shell calls it once on `enter` and once after each handled event.
- `draw` draws the body only. The shell clips to the body first.
- `hint` returns the footer text, or NULL for the default.

`dx_ctx_t` carries the drawing table, the theme and the current tick time. A minimal screen with a counter:

```c
static int count;

static unsigned event(void *st, const dx_ui_event_t *ev)
{
    (void)st;
    if (ev->type != DX_UI_SELECT) {
        return DX_UI_IGNORED;
    }
    count++;
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    char num[12];

    (void)st;
    dx_itoa(count, num);
    ctx->gfx->text(area.x + 4, area.y + 4, num, ctx->theme.fg, DX_COLOR_NONE, 2);
}

const dx_screen_t dx_screen_count = { "COUNT", NULL, NULL, event, NULL, draw, NULL, NULL };
```

### The main loop

Each step of `dx_app_step()`:

1. Reads the clock and the tick counter. Without a clock, the minute is "none".
2. Calls the screen's `tick` when its wake time has passed.
3. Redraws when the tab or the minute changed or a screen asked for it: clear, header, body inside the clip, footer, then one `present`.
4. Logs on serial after the `present`: `deitrix: ready` after the first frame, `deitrix: tab=<NAME>` after a frame for a new tab.
5. Reads the clock and the tick counter again, since the tick and the draw take time. Waits in `wait_event` until the earlier of the next whole minute and the screen's wake time, or with no timeout when there is neither. When the minute changed or the wake time passed during the draw, the timeout is 0 and the next step starts at once.
6. Translates the event and routes it.

The clock has whole seconds only, so the wait can end up to 1 s before the minute changes. Then the next step sees the same minute, does not redraw, and waits again, at most 1 s. The time is shown in UTC: Pip-OS has no time zone.

The framework allocates nothing. Widget state lives in structs the screen owns. The shell's state is static in `dx_main()`.

### Widgets

The list, text, gauge, key and value, and sub-tab widgets are structs the screen owns, with `dx_<name>_init()` and `dx_<name>_draw(widget, ctx, area)`. The list, the text block and the sub-tabs take keys through `dx_<name>_event()`, which returns `DX_UI_IGNORED` or `DX_UI_HANDLED | DX_UI_DIRTY`. The gauge and the key and value rows are read-only, so they have no event function.

- `list.h`: rows from a callback, each with a label, an optional value at the right and flags for disabled and marked. UP and DOWN move the selection, repeats too. At either end the list stays and returns `DX_UI_IGNORED`. The window scrolls by one row to keep the selection in view. The selected row is drawn inverse. A long label is cut with `~` before the value. An empty list draws a centred message. A scroll bar shows at the right when the rows do not fit.
- `text.h`: a block of text wrapped to the area on word boundaries by `dx_wrap()`. A word longer than a line is split, `\n` forces a break and a tab counts as a space. UP and DOWN scroll a line at a time, with the same scroll bar.
- `gauge.h`: a label, a bar for `value` of `max` and the numbers at the right. The value is clamped to 0 to `max`. `max` 0 draws an empty bar. The numbers sit right-aligned in a column of `value_cols` characters. Give every gauge of a group the same `value_cols`, from `dx_gauge_value_cols()`, and their bars share both edges.
- `kv.h`: read-only key and value rows, the values in one column after the longest key.
- `subtabs.h`: a row of up to 5 labels under the header. LEFT and RIGHT move between them. At either end they return `DX_UI_IGNORED`, so the shell changes the main tab. The active label is bright and underlined.
- `icon.h`: draws a 1-bit icon from `assets/` centred in an area. `dx_icon_find()` looks one up by its file name.
- `vfont.h`: a stroke font for large text: digits, A to Z, space and `: - . / %`. Each glyph is a few polylines on a grid 4 units wide and 6 high, drawn with `line` at any unit size. From unit 3 the strokes are 2 px wide. A character the font lacks draws as a crossed box. `dx_vfont_width()` measures a string.
- `frame.h`: the header and the footer every tab shares.

### The widget gallery

`make gallery` builds a second ROM into `build/gallery/`, with its own card at `build/gallery/pipos/sd.img`. It is for the QEMU tests only and never goes on the default card. `tests/gallery/gallery.c` replaces `src/screens/` with one screen per widget, so the same app shell runs it: LIST, TEXT, BAR, KV, TABS, ICON and FONT. The sample data is fixed. Each list and text move logs a `gallery:` line for the tests.

## Planned

These are plans, not code:

- Content files under `/PIPBOY/DATA/` on the card, from `content/`.
- Saves through the Pip-OS save calls.
- The five screens with real content: STAT, INV, DATA, MAP and RADIO.
- A holotape game.
