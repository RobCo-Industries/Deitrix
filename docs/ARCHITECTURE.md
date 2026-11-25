# Architecture

Deitrix is one ROM image. Pip-OS loads it at `0x00400000`, starts it in user mode and calls `pip_main()`. The ROM never returns.

## Source layout

### `src/main.c`

The entry and the main loop. `dx_main()` starts the ROM, then runs `dx_app_step()` forever. `src/app.h` declares the loop's state and the service interface, so the host tests can run it one step at a time.

### `src/content.c`

The one copy of the model the ROM keeps. `dx_content_load()` reads every content file once at start, `dx_content_load_saves()` applies the saves on top. Screens read the model through small accessors, such as `dx_content_items()`. See [The data model](#the-data-model).

### `src/platform/`

The only code that calls Pip-OS. `platform.c` defines `pip_main()` and wraps the system calls Deitrix uses behind `platform.h`: the scheme colour, the clock, the tick counter, `wait_event`, `log`, `tone`, the drawing table, whole-file reads, file handles, `dir_list`, the saves, the audio calls and the system information. It also wraps two libpip functions, `pip_ini_parse` and `pip_crc32`, as `dx_ini_parse` and `dx_crc32`. Nothing outside this folder includes a `<pipos/...>` header.

The host tests link `tests/host/fake_platform.c` in its place. It has an in-memory card, a save store, a recorded audio voice that plays against its tick counter, and a settable clock. Its `dx_ini_parse` and `dx_crc32` call RobCo-Core's `ini.c` and `crc32.c`, the code libpip wraps, so the host tests run the same parser as the ROM.

### `src/model/`

Pure data code, with no drawing and no system calls. `nav.c` moves between tabs with wrap-around. `clock.c` turns Unix seconds into the minute of the day, its `HH:MM` text and the wait to the next minute. The other files are the data model: see [The data model](#the-data-model).

### `src/services/`

The background services: the radio, the autosave and the uptime. See [Services](#services).

### `src/ui/`

The UI framework: the core types in `ui.h`, the frame every tab shares, and the widgets. See [The UI framework](#the-ui-framework).

### `src/screens/`

One file per tab, each defining one `dx_screen_t`: `stat.c`, `inv.c`, `data.c`, `map.c` and `radio.c`. `table.c` lists them in tab order. Every tab is built, see [Screens](#screens). `stat_draw.c` draws STAT's name and gauges. DATA keeps TAPES in `data_tapes.c` and CLOCK in `data_clock.c`.

### `holotapes/`

The holotapes that ship on the card, one folder each. See [Holotapes](#holotapes).

### `assets/`

The icons, as plain PBM files. `tools/pbm2c.py` turns them into `build/gen/icons.c` at build time. That file is not committed.

### `content/`

The sample content of the card: `PIPBOY/DATA/` with the profile, items, notes, maps, stations and tunes. `make card-tree` copies it to `build/card/` and adds the two PCM tracks from `tools/mkpcm.py`. `make sdimage` and `make bootdir` copy `build/card/` to the card root. The formats are in [CONTENT.md](CONTENT.md).

### `tests/`

`tests/host/` holds the C host tests, the fake platform and the `pbm2c.py` test. `tests/tools/` holds the tests of the content tools. `tests/gallery/` holds the widget gallery's screens. `tests/*.py` are the QEMU tests, with their screen goldens in `tests/golden/`. See [TESTING.md](TESTING.md).

## The UI framework

### The platform seam

`platform.h` declares `dx_gfx_t`, a table of drawing functions: `clear`, `line`, `rect`, `text`, `polyline`, `arc`, `blit1`, `clip` and `present`. Each call takes its own colour, so no drawing state carries over between calls. `platform.c` maps each one to the Pip-OS call that does it: it sets the colour with `set_color`, then draws. `blit1` fills a background rectangle first when one is given, because `blit_1bpp` draws set bits only. Arguments outside the kernel's limits draw nothing. `DX_COLOR_NONE` is a transparent text background.

The host fake gives a recorder instead. Each call becomes one line of a draw list, for example `text 8 4 "STAT" fg none s1`. Colours that match the theme print as `fg`, `dim`, `faint` or `bg`.

### The theme

`dx_theme_from_fg()` builds the theme from the scheme colour Pip-OS reports in `gfx_info`. `dim` is that colour at 1/2 per channel and `faint` at 1/4. `bg` is black. No other code names a colour.

### Events and routing

`dx_ui_translate()` turns a Pip-OS event into a `dx_ui_event_t`. A press gives an event and a repeat gives one with `repeat` set. A release gives none. A dial turn gives `DX_UI_DIAL` with its detents in `delta`. The dial push is SELECT. Button 6, the Back button of ABI 1.2, gives `DX_UI_BACK`.

The navigation rule:

- In a detail view or any nested level, BACK and LEFT both mean "back one level". The screen handles them. It handles RIGHT too, so no key changes the tab there.
- At the top level of a tab, LEFT and RIGHT move between sub-tabs. When the screen ignores them, the shell changes the tab. The shell ignores BACK, so BACK never changes the tab. An ignored BACK plays the deny sound, like an ignored UP, DOWN or SELECT.
- The dial push is SELECT. A dial turn always changes the tab.

A screen that keeps a depth tests `dx_ui_is_back()`. It is true for `DX_UI_BACK` and for LEFT, repeats too. So a device with a Back button and the QEMU console behave the same. On the console, Backspace or a lone Escape is Back, and LEFT works too.

The SDK seals the ROM for ABI 1.3. The loader checks only the major version, so the ROM still runs on a 1.0, 1.1 or 1.2 Pip-OS. A 1.0 or 1.1 Pip-OS never sends button 6, and LEFT is the only way back there.

The app shell routes each event:

1. A repeat of a latched key is dropped. See the held-key rule below.
2. A dial turn changes the tab by its detents, with wrap-around. The screen does not see it.
3. Every other event goes to the active screen. It returns `DX_UI_IGNORED`, or `DX_UI_HANDLED`, optionally with `DX_UI_DIRTY` to ask for a redraw and `DX_UI_LEVEL` when it opened or closed a nested view.
4. LEFT or RIGHT that the screen ignores moves to the previous or next tab.
5. Anything else the screen ignores does nothing.

The held-key rule is the same on every tab. It lives in `src/ui/latch.h`, and `dx_app_route()` applies it. A fresh press that changes the level latches its key: the screen returned `DX_UI_LEVEL`, or the shell changed the tab. The repeats of that key are then dropped until the next fresh event of any key, the dial included. So a held LEFT that closes a view does not go on to change the sub-tab or the tab, and a held SELECT that opens a view does not act inside it. Other repeats still reach the screen: UP and DOWN in lists and text, INV's count, RADIO's volume and the clock setter's fields. A screen opens or closes a level on a fresh press only, since a repeat can arrive with no press before it, as after a tab change by the dial.

A tab change calls `leave` on the old screen and `enter` on the new one, then redraws.

### Footer hints

The footer names the keys that do something in the current state. Every hint follows one style, set in `ui/frame.h`:

- Each group is `KEY: action`. The keys are `UP/DOWN`, `LEFT/RIGHT`, `SELECT`, `BACK` and `DIAL`.
- Two spaces separate the groups.
- Movement comes first, then SELECT, then BACK.
- A hint has at most 38 characters, so it keeps one column free on each side.
- No screen hint names the tab change. The header shows the tabs, and the dial always changes them. A hint for every state would not fit with the tab words in 38 characters. A state with no key of its own shows the shell's hint, `DIAL: change tab`.
- LEFT/RIGHT shows only where it does something other than paging, or where paging is the only thing to do. The sub-tab row shows the pages.

`tests/host/test_hints.c` walks every state on three cards and checks each hint against these rules and this table:

| Tab | State | Hint |
|---|---|---|
| STAT | STATUS, ABOUT, ATTRS with at most 1 attribute | `LEFT/RIGHT: page` |
| STAT | ATTRS with 2 or more attributes | `UP/DOWN: move` |
| INV | List | `UP/DOWN: move  SELECT: open` |
| INV | Empty category | `LEFT/RIGHT: page` |
| INV | Detail | `UP/DOWN: +/-  SELECT: mark  BACK: list` |
| INV | No items | `DIAL: change tab` |
| DATA | NOTES list | `UP/DOWN: move  SELECT: open` |
| DATA | NOTES, TAPES without entries | `LEFT/RIGHT: page` |
| DATA | Note open, longer than the screen | `UP/DOWN: scroll  BACK: list` |
| DATA | Note open, short | `BACK: list` |
| DATA | TAPES list, also with a result line such as "Tape ended, status 0." or "Save failed. SELECT again runs it." | `UP/DOWN: move  SELECT: run tape` |
| DATA | CLOCK | `SELECT: set time` |
| DATA | Setter | `UP/DOWN: set  SELECT: ok  BACK: cancel` |
| MAP | Two or more places | `UP/DOWN: place  SELECT: open` |
| MAP | One place | `SELECT: open` |
| MAP | No place, two maps | `LEFT/RIGHT: page` |
| MAP | No place and one map, or no map | `DIAL: change tab` |
| MAP | Detail | `BACK: map` |
| RADIO | Station | `UP/DOWN: move  SELECT: play` |
| RADIO | Station on air | `UP/DOWN: move  SELECT: stop` |
| RADIO | Volume | `UP/DOWN: move  LEFT/RIGHT: volume` |
| RADIO | UI sounds | `UP/DOWN: move  SELECT: sound on/off` |

### Sounds

`dx_ui_sound()` plays a short tone through the platform: MOVE, SELECT, BACK or DENY. It plays nothing when the owner turned UI sounds off in the settings, or while the radio plays. `dx_ui_sound_for()` picks it: MOVE for a tab change or a handled key that changed the screen, SELECT or BACK when the screen handled one, and DENY for UP, DOWN, SELECT or BACK that the screen ignored on a fresh press. Repeats never deny. `DX_UI_SOUND` in `ui.h` turns every tone off when set to 0.

### Layout

`ui.h` holds the bands of the screen. The skeleton's positions stay, so its goldens still match:

| Band | Rows |
|---|---|
| Header: the tab bar and the clock, with a rule at row 21 | 0 to 21 |
| Body | 22 to 214 |
| Footer: a rule at row 215 and the hint at row 220 | 215 to 239 |

A row of text is 16 px, the font cell. At scale 1 the screen holds 40 columns of 8 px.

Every tab lays out its body on one grid, `src/ui/grid.h`. Rows below count from the top of the body, row 22:

| Constant | Row | What sits there |
|---|---|---|
| `DX_GRID_BAND_H` | 0 to 19 | The sub-tab row, or a detail's title row, with a faint rule on its last line |
| `DX_GRID_BAND_TEXT_DY` | 1 | The text of that band |
| `DX_GRID_TOP` | 6 | The first content row when there is no band, as on RADIO |
| `DX_GRID_BAND_TOP` | 26 | The first content row under the band |
| `DX_GRID_STATUS_RULE_DY` | 171 | A faint rule over the status row |
| `DX_GRID_STATUS_DY` | 175 | A status or summary row: INV's summary, MAP's place, a tape's result |

`DX_GRID_GAP`, 6 px, is the gap under the header rule or the band. An empty-state message is centred in the area under the band, or in the body when there is no band.

The tab labels must end 8 px before the clock, at x 268. The header puts 8 px on each side of a label. When the labels do not fit, it narrows that space, down to 2 px. When they still do not fit, it cuts every label to 4 letters, then to 3. `dx_frame_tab_cut()` gives the letters it keeps. The limits are 8 tabs and 5 letters per name, and with them 3 letters always fit:

| Tabs of 5 letters | Letters shown |
|---|---|
| 1 to 6 | 5 |
| 7 | 4 |
| 8 | 3 |

`dx_app_init()` refuses a NULL table, a NULL entry, no screens, more than 8, or a name that is empty or longer than 5 letters. It checks every entry before it reads any. It logs `deitrix: refused: ...` and the ROM stops there.

### The screen interface

A screen is a `dx_screen_t`: a tab label of 1 to 5 letters, six callbacks and a state pointer. Any callback may be NULL.

- `enter` and `leave` run when the tab becomes active and when it stops.
- `event` gets each routed event.
- `tick` runs when the screen's wake time passes. It returns the next wake time in ticks, or 0 for none, and may set `DX_UI_DIRTY`. The shell calls it once on `enter` and once after each handled event. The tick counter wraps at 2^31, so a screen builds its wake time with `dx_ticks_add(now_ms, ms)`, at most one day ahead; the helper never returns 0.
- `draw` draws the body only. The shell clips to the body first.
- `hint` returns the footer text, or NULL for the shell's own. See [Footer hints](#footer-hints).

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
2. Calls each service's `tick`, then the screen's `tick`, when their wake times have passed.
3. Redraws when the tab or the minute changed, or a screen or a service asked for it: clear, header, body inside the clip, footer, then one `present`.
4. Logs on serial after the `present`: `deitrix: ready` after the first frame, `deitrix: tab=<NAME>` after a frame for a new tab.
5. Reads the clock and the tick counter again, since the tick and the draw take time. Waits in `wait_event` until the earliest of the next whole minute, the screen's wake time and the services' wake times, or with no timeout when there is none. When the minute changed or the wake time passed during the draw, the timeout is 0 and the next step starts at once. The loop compares and subtracts ticks only through `dx_ticks_due` and `dx_ticks_since`, which mask the difference to 31 bits as the Pip-OS ABI asks.
6. Translates the event and routes it.

The clock has whole seconds only, so the wait can end up to 1 s before the minute changes. Then the next step sees the same minute, does not redraw, and waits again, at most 1 s. The time is shown in UTC: Pip-OS has no time zone.

The framework allocates nothing. Widget state lives in structs the screen owns. The shell's state is static in `dx_main()`.

### Widgets

The list, text, gauge, key and value, and sub-tab widgets are structs the screen owns, with `dx_<name>_init()` and `dx_<name>_draw(widget, ctx, area)`. The list, the text block and the sub-tabs take keys through `dx_<name>_event()`, which returns `DX_UI_IGNORED` or `DX_UI_HANDLED | DX_UI_DIRTY`. The gauge and the key and value rows are read-only, so they have no event function.

- `list.h`: rows from a callback, each with a label, an optional value at the right and flags for disabled and marked. UP and DOWN move the selection, repeats too. At either end the list stays and returns `DX_UI_IGNORED`. The window scrolls by one row to keep the selection in view. The selected row is drawn inverse. A long label is cut with `~` before the value. An empty list draws a centred message. A scroll bar shows at the right when the rows do not fit.
- `text.h`: a block of text wrapped to the area on word boundaries by `dx_wrap()`. A word longer than a line is split, `\n` forces a break and a tab counts as a space. UP and DOWN scroll a line at a time, with the same scroll bar. It keeps at most 128 lines and reads at most 256 KiB; a longer text ends with a `[text cut]` line.
- `gauge.h`: a label, a bar for `value` of `max` and the numbers at the right. The value is clamped to 0 to `max`. `max` 0 draws an empty bar. The numbers sit right-aligned in a column of `value_cols` characters. Give every gauge of a group the same `value_cols`, from `dx_gauge_value_cols()`, and their bars share both edges. A selected gauge draws its row inverse, like a list's selected row. `value_only` shows the value without `/max`, and `pad` moves the label in from the left edge.
- `kv.h`: read-only key and value rows, the values in one column after the longest key.
- `subtabs.h`: a row of up to 5 labels under the header. LEFT and RIGHT move between them. At either end they return `DX_UI_IGNORED`, so the shell changes the main tab. The active label is bright and underlined.
- `icon.h`: draws a 1-bit icon from `assets/` centred in an area. `dx_icon_find()` looks one up by its file name.
- `vfont.h`: a stroke font for large text: digits, A to Z, space and `: - . / %`. Each glyph is a few polylines on a grid 4 units wide and 6 high, drawn with `line` at any unit size. From unit 3 the strokes are 2 px wide. The dots of `:` and `.` are filled squares, drawn with `rect`, as wide as the strokes around them would be. A character the font lacks draws as a crossed box. `dx_vfont_width()` measures a string.
- `frame.h`: the header and the footer every tab shares.

### The widget gallery

`make gallery` builds a second ROM into `build/gallery/`, with its own card at `build/gallery/pipos/sd.img`. It is for the QEMU tests only and never goes on the default card. `tests/gallery/gallery.c` replaces `src/screens/` with one screen per widget, so the same app shell runs it: LIST, TEXT, BAR, KV, TABS, ICON and FONT. The sample data is fixed. SELECT on a LIST row opens its detail, one level down, and BACK or LEFT comes back. Each list and text move and each detail key logs a `gallery:` line for the tests.

## The data model

### Modules

Each content file has one module in `src/model/`. Each has a `load` function that takes the file's bytes, or the error from `dx_plat_read_file`, and fills a struct with fixed tables sized from the limits in [CONTENT.md](CONTENT.md). Nothing is allocated. A file that is ignored leaves its struct empty, with the reason in its `dx_load_t`: missing, too large, a read error, a parse error and its line, or a bad version.

| Module | File | What it keeps |
|---|---|---|
| `profile.c` | PROFILE.INI | The owner's name and line, up to 6 gauges, 12 attributes and 8 about lines |
| `items.c` | ITEMS.INI | Up to 5 categories and 64 items. The overlay: a count and a marked flag per item. `dx_items_set_count`, `dx_items_add` and `dx_items_toggle_marked` change it, `dx_items_total` sums a category |
| `notes.c` | NOTES/ | Up to 32 notes in name order, each with its file name and title. `dx_notes_body` turns a note's bytes into its body |
| `map.c` | MAP.INI | The local and world maps: box, title, up to 64 lines and 24 places each. `dx_map_fit` fits a box into a pixel rectangle |
| `radio.c` | RADIO.INI and RADIO/ | Up to 6 stations. A tune's notes, parsed once by `dx_tun_parse`. Whether each station can play |
| `saves.c` | DEITRIX.SAV, INV.SAV | The settings, and the encoding and decoding of both saves |
| `load.c` | all INI files | The shared INI loading: the size, the parse, the version key, the reason text |
| `util.c` | | Strings, numbers and names, with no C library |

The INI modules use one rule for repeats: keys arrive in file order and the last one wins. So a repeated `g1` replaces the first, even when it is bad. A repeated section merges.

The overlay and the settings each keep a `changes` counter. Every change that alters a value adds one. The autosave service watches the counters.

`dx_map_fit` uses integers only. It picks the smaller of the two scales, `(w - 1) / box width` and `(h - 1) / box height`, as a fraction `num / den`. It centres the box on the other side. `dx_map_to_px` rounds each point to the nearest pixel, so the corners land on the rectangle's edges.

### Loading at start

`dx_content_load()` reads the files in this order: PROFILE.INI, ITEMS.INI, the notes, MAP.INI, RADIO.INI, then each station's file. Each file is independent: one that is ignored does not stop the others. Every file but a PCM track goes through one static 32 KiB buffer. The notes list comes from `dir_list`, 16 entries per call; the ROM reads each listed note once for its title. A PCM station takes its size from the listing of `RADIO/` and is not read at start.

Each file logs one line on serial:

```
deitrix: load ITEMS.INI items=14 categories=4
deitrix: load MAP.INI ignored: bad version
deitrix: load RADIO/HEARTH.PCM bytes=256000
deitrix: load DEITRIX.SAV ok tab=4 volume=80 sound=1 station=1
```

### Startup order

`dx_main()` starts the ROM in this order:

1. The app shell: the drawing table and the theme, then the screen table check.
2. The content, as above.
3. The saves, on top of the content. The volume default is the level Pip-OS had at start. The volume and the UI sound setting apply at once.
4. The services: the radio, the autosave and the uptime.
5. The tab from the settings. Then the first frame and `deitrix: ready`.

## Services

A service is a `dx_service_t`: a name, `init`, `tick` and `stop`. `dx_app_start_services()` takes a table of up to 4. The shell calls a service's `tick` when its wake time passes, and once after every routed event, so a service sees a change at once. `tick` returns the next wake time, built with `dx_ticks_add()`, or 0 for none. The loop's wait is the earliest of the next minute change, the screen's wake time and every service's wake time.

A service asks for a redraw the way a screen does: `tick` sets `DX_UI_DIRTY` in its `flags` argument when something a screen may show has changed. The shell then redraws the tab shown in the same step, with one `present`. It redraws whatever tab is shown, even one that shows nothing of the service. Such changes are rare, so the extra frame costs little. A tick that changes nothing visible leaves the flags alone and costs no frame: the streaming wakes every 250 ms draw nothing. A screen that calls a service directly, such as `dx_radio_play`, does so in its event handler, which already returns `DX_UI_DIRTY`.

### The radio

The radio plays whatever tab is shown. Its states are off, playing a tune and playing a PCM track. The screen calls `dx_radio_play`, `dx_radio_stop`, `dx_radio_current`, `dx_radio_failed` and `dx_radio_set_volume`. `dx_radio_failed` gives the station an error stopped: its file was missing or could not be opened, read or played. A start or a stop clears it.

- A tune goes to `play_seq` in one call. The service wakes when the tune's length has passed. If `audio_status` says the sound still plays, it checks again every 50 ms. Then it starts the tune again.
- A PCM track streams from a file handle kept open. Chunks are 8000 bytes, 1000 ms each at 8000 Hz. The first goes to `sample`, the rest to `sample_queue`. One chunk plays, one waits in Pip-OS and one waits in the ROM's buffer. File handles cannot seek, so at the end of a looping track the ROM closes the file, opens it again and fills the rest of the chunk from the first byte. Every chunk is whole, so the joint has no gap.
- While a track streams, the service wakes every 250 ms, a quarter of a chunk. `sample_queue` takes a chunk only once the one before starts to play. So the slot opens at most 250 ms before the service fills it, and at least 750 ms of sound are still queued. In the steady state a wake reads at most one chunk from the card.
- `-PIP_EBUSY` keeps the chunk for the next wake. When `audio_status` says nothing plays before a chunk goes out, the queue ran dry: the service logs `deitrix: radio underrun` and that chunk starts the sound again.
- A track with `loop = no` ends after its last chunk: the radio goes off.
- A tick asks for a redraw when the state, the station or the underrun count changed: the end of a track, a read error that stops it, a failed tune restart, an underrun. A tune's replay and a chunk handed over ask for none. The RADIO tab logs `deitrix: radio shows <line>` when a frame shows another station or off.

Pip-OS has one voice, and a UI tone would replace the radio. So while the radio plays, it blocks the UI tones with `dx_ui_sound_block()`. They come back when it stops.

### The autosave

The autosave writes DEITRIX.SAV and INV.SAV when their counters change. It writes 2 s after the last change, and at most 10 s after the first one, so a burst of changes writes once. It writes only the saves that changed. Each write logs `deitrix: save DEITRIX.SAV ok` or `failed`. A failed write is tried again 5 s later. After 3 failed retries it gives up and logs `deitrix: save gave up` once. 

The unsaved mark tells the owner that changes are not on the card. It is a `!` in the normal colour, just left of the clock. It does not blink. It shows once a change has waited 12 s without reaching the card, `DX_SAVE_MARK_MS`: the longest wait of a healthy save, 10 s, plus 2 s. So a card that works never shows it. It stays while the changes are not saved, also after the autosave gave up. It goes as soon as every change is on the card, after the next change or a holotape's save. The service's tick turns it on and off, asks for a redraw and logs `deitrix: unsaved mark on` or `off`. It then stops its retries, so a broken card is not hit again and again. The changes stay marked as not saved. A new change to that save starts a fresh round of retries. `dx_autosave_flush()` writes at once. It tries every save that is not on the card once, even after a give-up. It returns 0 only when every save is on the card. The service's `stop` calls it. `dx_autosave_unsaved()` returns 1 while a change is not on the card. `dx_autosave_pending()` returns 1 only while the service plans a write, so it is 0 after a give-up. A write in QEMU takes 20 to 120 ms, and the UI waits only that long. No screen shows the saves, so the autosave never asks for a redraw.

### The uptime

`src/services/uptime.c` samples `ticks_ms` at least once an hour, `DX_UPTIME_SAMPLE_MS`, and counts its wraps at 2^31. A fall of more than 2^30 between two samples is a wrap. `dx_uptime_s()` gives the whole seconds since the start and samples too. It divides without 64-bit arithmetic, since the ROM has no libgcc.

## Screens

### STAT

`src/screens/stat.c` has three sub-tabs: STATUS, ATTRS and ABOUT. LEFT and RIGHT move between them. At STATUS, LEFT goes to the previous tab, and at ABOUT, RIGHT goes to the next. The sub-tab and the ATTRS selection stay when the tab is left, so a return shows the same view. No view of STAT changes state, so it writes no save.

- STATUS shows the owner's name, the owner's line in dim text under it, then up to 6 gauges in file order. The gauges share one label column and one value column. The name uses the stroke font at unit 4, or unit 3 when 4 does not fit in 312 px. A name that fits neither, or holds a character the stroke font lacks, uses the normal font. `dx_stat_name_unit()` makes that choice. UP, DOWN, SELECT and Back do nothing, and the shell plays the deny tone.
- ATTRS shows up to 12 attributes as a list: the name at the left, the value at the right. Ten rows fit, so 11 or 12 scroll. UP and DOWN move the selection. SELECT does nothing and denies. With no attributes it shows "No attributes in the profile.".
- ABOUT shows the device data as key and value rows: the Deitrix version, the Pip-OS version and ABI, the SoC, the RAM, the uptime and the time in UTC. Under them it shows the profile's about lines in dim text, as many as fit: 3. The keys do nothing there, except LEFT and RIGHT.

When PROFILE.INI is missing, STATUS and ATTRS show "No profile on the card.". When the file was ignored, they show "The profile was not read." and the reason, such as "PROFILE.INI line 12". ABOUT still shows the device data.

The device rows come from `dx_plat_sysinfo()`. "Pip-OS" is `pipos_version` and "ABI" is `abi_version`. "SoC" is `soc` as BCM2835, BCM2836 or BCM2837. "RAM" is `mem_size`: the RAM the firmware gives the ARM, in MiB. The Deitrix version is `VERSION` in the Makefile, the version the SDK seals the ROM with. The Makefile passes it to the code as `DX_VERSION`. `src/model/stat_fmt.c` writes every value as text.

The uptime is shown as `d hh:mm:ss`. Pip-OS's only uptime source is `ticks_ms`, which wraps at 2^31 ms, 24 days 20:31:23. The uptime service counts the wraps, see [The uptime](#the-uptime), so the page shows the true uptime past that. The time comes from `get_time`, or reads "no clock".

Only ABOUT ticks. Its tick wakes on each whole second of `ticks_ms` and asks for a redraw when the uptime second or the clock changed. On STATUS or ATTRS the tick returns 0, so the shell stops it. The shell ticks the screen after each event, so a move to ABOUT starts it at once. A frame of ABOUT reads the clock itself, so the time can show up to 1 s late.

The tab logs one line when a frame shows another view, just before the present. The first STATUS frame after the tab opens logs nothing:

```
deitrix: stat shows ATTRS sel=3 top=0
deitrix: stat shows STATUS
deitrix: stat about Deitrix=0.1.0 | Pip-OS=7.1.0 | ABI=1.3 | SoC=BCM2836 | RAM=960 MiB | Uptime=0d 00:00:12 | Time=no clock
```

ABOUT logs its line when the page shows and when the uptime's minute changes, not on every frame. A test that needs a newer value leaves the page and comes back. The profile's gauge data is `dx_profile_gauge_t` in `model/profile.h`, apart from the widget's `dx_gauge_t`. `src/screens/stat_draw.c` draws the name and the gauges.

### INV

`src/screens/inv.c` shows ITEMS.INI. `src/model/inv_fmt.c` holds its pure helpers: which item is row N of a category, and the text of a count, a weight and a summary.

The views:

- **The list.** The sub-tabs show the category labels in file order. Under them, the items of the active category in file order, 9 rows at a time. A row shows the name and, at the right, the count and unit, such as `20 m`. A count of 0 is dim. A marked item has the list's mark. A dim row under the list gives the entry count and the total weight of the category, such as `4 entries, 760 g`. The total uses `dx_items_total`. Below 1000 g it shows grams, else kilograms with one decimal, rounded half up to 100 g. When no item of the category has a weight, the row shows the entry count only.
- **The detail.** SELECT on a row opens it. It shows the name, then Count, Weight when the item has one, Category and Marked, then the note in dim text. The note never scrolls, since UP and DOWN change the count. A note longer than the space ends with `~`.
- **Empty states.** Without a loaded ITEMS.INI, or with no category, the body says "No items on the card." and, for a file that did not load, the reason, such as `ITEMS.INI missing`. A category without items says "Nothing here.".

The keys:

| View | Key | Does |
|---|---|---|
| List | UP, DOWN | Move the selection, repeats too |
| List | LEFT, RIGHT | Change the category. At either end the shell changes the tab |
| List | SELECT | Opens the detail of the selected item |
| Detail | UP, DOWN | Add or take one from the count, one per press or repeat, within 0 to 999 |
| Detail | SELECT | Marks or unmarks the item |
| Detail | BACK, LEFT | Back to the list, on the same row |
| Detail | RIGHT | Nothing: it cannot change the tab |

At 0 or 999 the model refuses the step. The tab ignores the key, so the shell plays the deny tone for a press and nothing for a repeat. BACK at the list is ignored too.

The tab keeps the active category and the selected row of each category while the ROM runs. Leaving the tab closes the detail, so coming back shows the list.

Changes go through `dx_items_add` and `dx_items_toggle_marked`. They count a change in the overlay, and the autosave writes INV.SAV. A refused step counts none.

The footer hints are in [Footer hints](#footer-hints). The state without items keeps the shell's hint.

Each key the tab takes logs one serial line before the redraw, for the QEMU tests: `deitrix: inv cat=1 row=0`, `open=knife`, `count=3`, `refused count=999`, `marked=1`, `back row=0`, `end row=0` at a list end, and `idle` for a key that changes nothing. LEFT or RIGHT that changes the tab logs no `inv` line.

### DATA

`src/screens/data.c` has three sub-tabs: NOTES, TAPES and CLOCK. It holds the sub-tabs and NOTES. `data_tapes.c` holds TAPES and `data_clock.c` holds CLOCK. LEFT and RIGHT move between the sub-tabs at the top level. At NOTES, LEFT goes to the previous tab, and at CLOCK, RIGHT goes to the next. Leaving the tab closes an open note or the setter, and drops the TAPES result line and the CLOCK status line. The sub-tab and the list selections stay.

SELECT, BACK and LEFT open, close, store or launch only on a fresh press. Their repeats do nothing, and a held LEFT does not change the sub-tab or the tab from a nested level. Opening or closing a note or the setter returns `DX_UI_LEVEL`, so the shell's held-key rule drops the repeats of that key until the next press. UP and DOWN act on every repeat in the lists, the note text and the setter fields.

NOTES lists the notes the model loaded, in name order, by title. Ten rows fit. UP and DOWN move the selection. SELECT opens the note: its title, a faint rule, then the body in the text widget, 38 columns and 9 lines. UP and DOWN scroll it. A body over 128 wrapped lines ends with `[text cut]`. The body comes from `dx_content_note_body` into a 4 KiB buffer the screen owns. A read error shows "The note could not be read." under the title. BACK or LEFT returns to the list with the same note selected. RIGHT does nothing there. With no notes the list shows "No notes on the card.". When the folder could not be read it shows "The notes were not read." and the reason.

TAPES lists the holotapes from `dx_plat_holo_list`, the seam's `holo_enumerate`. A row shows the title, or the file name when the title is empty, and the kind at the right: GAME, UTIL or DATA. The selected tape's 32x32 icon sits at the top right when it has one. The list is read when the sub-tab opens and after each launch, and the selection stays on the same file. With no tapes it shows "No holotapes on the card.". When the listing fails it shows "The holotapes could not be read.".

SELECT runs the selected tape:

1. A data tape is not run: the result line says "Data tapes cannot be started.". Pip-OS would refuse it with `-PIP_ENOEXEC`, and that answer for a data tape gives the same line.
2. `dx_autosave_flush()` writes any change that is not on the card, so a tape that crashes the device loses nothing. This includes changes the autosave gave up on. When a write fails, the tape does not run and the result line says "Save failed. SELECT again runs it.". The radio keeps playing. The next fresh SELECT runs the tape even if the write fails again. Moving the selection or leaving the sub-tab or the tab drops that line and the second chance. A held SELECT's repeats never count. A card that cannot be written must not block every tape, and the user must know the changes are not safe first.
3. `dx_radio_stop()` stops the radio. Pip-OS stops the sound when a tape starts, so the service must not think it still plays. After the tape the radio stays off.
4. `dx_plat_holo_launch(file, "")` runs the tape. The ROM waits inside the call.
5. The listing is read again and the whole frame redraws, since the back buffer holds what the tape drew.

The result line shows under the list until the selection moves or the sub-tab is left: "Tape ended, status N." for an exit status, "Tape was stopped by the system." for a fault, "Not a holotape." for a file that fails validation, "Card error." for a read error, "Tape not found." when the file vanished, and "The tape could not start." for any other error.

CLOCK shows the time as `HH:MM:SS` in the stroke font at unit 6, the date and the weekday under it, and "The clock runs in UTC.". `src/model/data_time.c` computes the weekday and converts Unix seconds to fields and back for 2000 to 2099. Without a clock the page shows `--:--:--` and "No clock. SELECT sets the time.". The tick reads the clock every 250 ms and asks for a redraw only when the second changed. On NOTES and TAPES the tick returns 0.

SELECT opens the setter: year, month, day, hour and minute in one row, the active field inverse. It starts from the clock, or from 2026-01-01 00:00 when there is no clock or its year is outside 2000 to 2099. UP and DOWN change the active field, repeats too. Each field wraps at its ends: year 2000 to 2099, month 1 to 12, day 1 to the month's length, hour 0 to 23, minute 0 to 59. A change of year or month clamps the day. RIGHT moves to the next field and LEFT to the previous one. On the first field a fresh LEFT cancels, like BACK; a held LEFT stops there. SELECT stores the time with 0 seconds through `dx_plat_set_time`, the seam's `set_time`. Pip-OS also writes the clock chip when one answered at boot. On success the setter closes and the page shows "Time set." with "Lost at power-off without a clock chip." in dim text under it. The ABI does not say whether a chip is present, so the second line is true either way. A failed chip write shows "The clock chip did not take the time." and keeps the setter open. The clock keeps the new time then, as Pip-OS documents.

The tab logs one line when a frame shows another view, just before the present. A launch logs the file before the call and the result after it. A store logs the seconds and the result:

```
deitrix: data notes sel=2 top=0 count=4
deitrix: data note SHOP01.TXT top=0 lines=12
deitrix: data tapes sel=0 top=0 count=2 | Tape ended, status 42.
deitrix: data launch LINETEST.HTP
deitrix: data tape rc=42
deitrix: data clock 09:30:07 2026-11-23 Monday | 
deitrix: data setter field=1 2026-02-01 07:00 | 
deitrix: data set_time 1769929200 rc=0
```

The CLOCK page logs once a second while the time runs. DATA writes no save. The only state it changes outside the screen is the clock, through `set_time`.

### MAP

`src/screens/map.c` shows the maps of MAP.INI, one sub-tab per map in file order: LOCAL, then WORLD. A card with one map has one sub-tab. LEFT and RIGHT move between them. At the first sub-tab LEFT goes to DATA, and at the last RIGHT goes to RADIO. No view of MAP changes state, so it writes no save. Nothing ticks: the screen redraws only after a key.

- The map view draws the map's lines in dim, fitted into the pane between the sub-tab row and the status row with `dx_map_fit()`. The fit keeps the aspect ratio, centres the map and leaves a margin of 8 px. The pane is the clip rectangle while the map draws. Each place is a 5 px square in fg. The selected place is a reticle: a 9 px ring around a 3 px dot, with a 3 px tick out of each side. The map's title sits at the right of the sub-tab row in dim text. Under a faint rule, the status row shows the selected place's name at the left and its kind at the right in dim text. UP and DOWN cycle the places in file order and wrap at both ends. SELECT opens the detail. With no places the row says "No places on this map.", and UP, DOWN and SELECT deny. With one place UP and DOWN deny too. A map with places and no lines shows the places alone.
- The detail shows the place's name in the title row, then four key rows: the kind, the position as the file gives it, the map's title and the place's number, such as "2 of 7". Under them a locator shows the whole map in a faint frame, the other places dim and the place as the reticle. Back or LEFT returns to the map with the same place selected. RIGHT does nothing and stays in the detail. UP, DOWN and SELECT deny.

The format has no owner position and no unit, so the detail shows no distance. It has no note per place either.

When MAP.INI is missing, ignored or holds no valid map, the body shows "No map on the card." and a dim reason, such as "MAP.INI missing", "MAP.INI line 4" or "MAP.INI holds no map.". The keys do nothing there, and LEFT and RIGHT change the tab.

Leaving the tab closes the detail. The sub-tab and each map's selected place stay.

The hint names the keys that do something now. See [Footer hints](#footer-hints). A single map without places and the empty state use the shell's hint.

`src/model/map_view.c` holds the pure helpers: the pane fit with its margin, a line in pixels, the place cycling, the marker squares and the position text. A map point lands on the pixel `dx_map_to_px()` gives: rounded to the nearest, halves up.

The tab logs one line when a frame shows another view, just before the present, and the time the map view took after each map frame. The first frame after the tab opens logs nothing:

```
deitrix: map shows LOCAL place=2
deitrix: map drew 0 ms
deitrix: map detail LOCAL place=2
deitrix: map shows none
```

The time comes from `ticks_ms`, which moves in 10 ms steps in QEMU. With the largest map the format allows, 64 lines of 16 points and 24 places, each redraw of the map view logged 0 ms on raspi0 and raspi2b in QEMU, and under 100 ms on every board.

### RADIO

`src/screens/radio.c` is the control panel of the radio service. It has one page and no sub-tabs.

- **The stations.** One row per station of RADIO.INI, in file order. A station whose file is missing or broken is dim. The station on air shows `ON AIR` at the right.
- **Now playing.** Under a faint rule, two rows. While a station plays: "Playing" and its name, then its kind in dim text: `tune` for `kind = tune`, `recording` for `kind = pcm`. When nothing plays: "Radio off." in dim text. When the last start failed or an error stopped the station: "Station has nothing to play." and the reason, such as `LULLABY.TUN missing`.
- **The settings.** Under a second rule, `Volume` with a bar and the level, drawn by the gauge widget, and `UI sounds` with On or Off. Their labels are dim, as on STAT's gauges.

UP and DOWN move one cursor through the stations, then Volume, then UI sounds. They stop at both ends. The selected row is inverse.

| Row | Key | Does |
|---|---|---|
| Station | SELECT | Starts it, or stops it when it plays. Starting one stops the station before |
| Station | LEFT, RIGHT | Not handled: the shell changes the tab |
| Volume | LEFT, RIGHT | The level goes to the next multiple of 10 down or up, within 0 to 100. Repeats too. At a limit a press plays the deny tone and the tab stays |
| UI sounds | SELECT, LEFT, RIGHT | Turns UI sounds on or off. A repeat does nothing and the tab stays |

BACK does nothing on any row. The service plays on whatever tab is shown, so leaving the tab does not stop the radio. The cursor stays while the ROM runs.

The tab reads the stations from `dx_content_radio()`, the settings from `dx_content_settings()`, and what plays from `dx_radio_current()` and `dx_radio_failed()`. A start sets the last station with `dx_settings_set_station`. A stop keeps it. The volume goes through `dx_settings_set_volume` and `dx_radio_set_volume`, the UI sounds through `dx_settings_set_ui_sound` and `dx_ui_sound_enable`. Each setter counts a change only when the value changes, and the autosave writes DEITRIX.SAV. A refused step counts none.

At start the cursor goes to the last station. The radio does not resume: the station is only selected. Without stations the cursor starts on Volume.

When the service changes what plays, such as at the end of a track played once, its tick asks for a redraw. The tab has no tick of its own.

Without RADIO.INI, or with a file the loader ignored, the list area says "No stations on the card." and the reason in dim text, such as `RADIO.INI missing`. The settings still work.

The footer hints are in [Footer hints](#footer-hints).

Each key the tab takes logs one serial line before the redraw: `deitrix: radio row=3`, `end row=0`, `station=4`, `off`, `volume=70`, `refused volume=100`, `sound=0`, and `idle` for a key that changes nothing. A start that fails adds `deitrix: radio cannot play`. A frame that shows another state logs `deitrix: radio shows Playing Hearth`, `shows Off` or `shows Cannot play Lullaby`, just before the present.

## Holotapes

A holotape is a separate image. DATA's TAPES list starts it with `holo_launch`, see [DATA](#data). Each one lives in its own folder under `holotapes/` with a Makefile that includes the SDK's `pipos-app.mk` with `KIND := holotape`. `make holotapes` builds them into `build/holo/`, and `make` builds them with the ROM. The top Makefile passes them to Pip-OS's `TAPE=` list, so `make sdimage`, `make run` and `make bootdir` put them in `/PIPBOY/HOLO/`.

### Culvert

Culvert is a pipe-turning puzzle, a game tape of about 14 KiB. Its save `CULVERT.SAV` sits in `/PIPBOY/SAVE` beside DEITRIX.SAV and INV.SAV, and none of them reads another. [CULVERT.md](CULVERT.md) says how to play it.

| File | Role |
|---|---|
| `logic.c`, `logic.h` | Pieces, the level generator, the flood fill, the solved test, the seed, the timer and the save record. Pure. |
| `game.c`, `game.h` | The state machine: title, play, pause, solved, exit. Pure. |
| `draw.c`, `draw.h` | The screens, through the drawing calls |
| `main.c` | `pip_main()`, the event loop, the log lines, the sounds and the save calls |
| `icon.pbm` | The 32x32 META icon, a pipe elbow |

The pure files make no system call and keep no global, so the host tests link them. `game.c` takes an event or a tick and sets effect bits and one log code: what to draw, which sound to play, whether to save or exit. `main.c` acts on them. It waits with `wait_event` and a timeout that ends at the next change on screen: the next second of the clock while a level runs, the next flourish frame after a solve, forever on the title and the pause menu.

A piece is a 4-bit mask of its openings: north 1, east 2, south 4, west 8. A clockwise quarter turn rotates the mask one bit left.

The level number fixes the puzzle:

1. The seed is MurmurHash3's 32-bit finaliser applied to the level number XOR `0x43554C56`, and never 0.
2. The size class is `(level - 1) / 3`, at most 8. The classes are 5x4, 6x4, 6x5, 7x5, 8x5, 8x6, 9x6, 10x6 and 10x7.
3. The source is the tile at `(w / 2, h / 2)`.
4. A random spanning tree grows from the source. Each step takes a random edge from the tree to a tile outside it, with xorshift32 seeded by the seed. A tile's piece is its set of tree edges. This is the solution.
5. Each tile turns a random 0 to 3 quarters. If the level is still solved, the first tile that is not a cross turns once more.

The flood fill goes from the source through openings that meet an opening of the neighbour. The level is solved when the fill reaches every tile and no opening faces a wall or a closed side.

The tile size is the largest multiple of 4 that fits the grid in 296x176 pixels, at most 40. It is 40 pixels at 5x4 and 24 at 10x7. The title row takes the top 28 pixels, the status row the bottom 28.

The save is `CULVERT`, 30 bytes, little-endian:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `CVLT` |
| 4 | 1 | Version, 1 |
| 5 | 1 | Size classes, 9 |
| 6 | 2 | Highest level reached, 1 or more |
| 8 | 18 | Fewest moves per size class, `0xFFFF` for none |
| 26 | 4 | CRC-32 of bytes 0 to 25 |

A record of another size, magic, version or class count, a bad CRC or a level of 0 is ignored, and the game starts fresh.

Pip-OS empties the event queue when the tape starts. A SELECT still held from the launch then sends only repeats and its release. The game acts on a press, and only the arrows repeat, so the title waits for a fresh key.

The tape logs one line per step on the serial console, after the frame is on screen: `culvert: title`, `culvert: level n= w= h= seed=`, `culvert: cursor x= y=`, `culvert: turn x= y= connected=`, `culvert: menu item=`, `culvert: pause`, `culvert: resume`, `culvert: solved level= moves=`, `culvert: done` when the message shows, `culvert: idle` for a key that changes nothing, and `culvert: exit rc=0`. It also logs `culvert: load rc=` and `culvert: save rc=`. Before the exit line, `culvert stats: grid draws= ms=` gives the number of full grid redraws and their total time from `ticks_ms`.
