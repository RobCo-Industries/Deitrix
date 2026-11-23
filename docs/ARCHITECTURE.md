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

The background services: the radio and the autosave. See [Services](#services).

### `src/ui/`

The UI framework: the core types in `ui.h`, the frame every tab shares, and the widgets. See [The UI framework](#the-ui-framework).

### `src/screens/`

One file per tab, each defining one `dx_screen_t`: `stat.c`, `inv.c`, `data.c`, `map.c` and `radio.c`. `table.c` lists them in tab order. STAT and INV are built, see [Screens](#screens). DATA, MAP and RADIO draw the shared placeholder from `placeholder.c`: the tab's name in large text, "Not built yet." and one or two lines from the model. SELECT on RADIO cycles the radio: off, station 1, station 2 and so on, then off.

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

The SDK seals the ROM for ABI 1.2. The loader checks only the major version, so the ROM still runs on a 1.0 or 1.1 Pip-OS. That Pip-OS never sends button 6, and LEFT is the only way back.

The app shell routes each event:

1. A dial turn changes the tab by its detents, with wrap-around. The screen does not see it.
2. Every other event goes to the active screen. It returns `DX_UI_IGNORED`, or `DX_UI_HANDLED`, optionally with `DX_UI_DIRTY` to ask for a redraw.
3. LEFT or RIGHT that the screen ignores moves to the previous or next tab.
4. Anything else the screen ignores does nothing.

A tab change calls `leave` on the old screen and `enter` on the new one, then redraws.

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
- `gauge.h`: a label, a bar for `value` of `max` and the numbers at the right. The value is clamped to 0 to `max`. `max` 0 draws an empty bar. The numbers sit right-aligned in a column of `value_cols` characters. Give every gauge of a group the same `value_cols`, from `dx_gauge_value_cols()`, and their bars share both edges.
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
4. The services: the radio and the autosave.
5. The tab from the settings. Then the first frame and `deitrix: ready`.

## Services

A service is a `dx_service_t`: a name, `init`, `tick` and `stop`. `dx_app_start_services()` takes a table of up to 4. The shell calls a service's `tick` when its wake time passes, and once after every routed event, so a service sees a change at once. `tick` returns the next wake time, built with `dx_ticks_add()`, or 0 for none. The loop's wait is the earliest of the next minute change, the screen's wake time and every service's wake time.

A service asks for a redraw the way a screen does: `tick` sets `DX_UI_DIRTY` in its `flags` argument when something a screen may show has changed. The shell then redraws the tab shown in the same step, with one `present`. It redraws whatever tab is shown, even one that shows nothing of the service. Such changes are rare, so the extra frame costs little. A tick that changes nothing visible leaves the flags alone and costs no frame: the streaming wakes every 250 ms draw nothing. A screen that calls a service directly, such as `dx_radio_play`, does so in its event handler, which already returns `DX_UI_DIRTY`.

### The radio

The radio plays whatever tab is shown. Its states are off, playing a tune and playing a PCM track. The screen calls `dx_radio_play`, `dx_radio_stop`, `dx_radio_current`, `dx_radio_state` and `dx_radio_set_volume`.

- A tune goes to `play_seq` in one call. The service wakes when the tune's length has passed. If `audio_status` says the sound still plays, it checks again every 50 ms. Then it starts the tune again.
- A PCM track streams from a file handle kept open. Chunks are 8000 bytes, 1000 ms each at 8000 Hz. The first goes to `sample`, the rest to `sample_queue`. One chunk plays, one waits in Pip-OS and one waits in the ROM's buffer. File handles cannot seek, so at the end of a looping track the ROM closes the file, opens it again and fills the rest of the chunk from the first byte. Every chunk is whole, so the joint has no gap.
- While a track streams, the service wakes every 250 ms, a quarter of a chunk. `sample_queue` takes a chunk only once the one before starts to play. So the slot opens at most 250 ms before the service fills it, and at least 750 ms of sound are still queued. In the steady state a wake reads at most one chunk from the card.
- `-PIP_EBUSY` keeps the chunk for the next wake. When `audio_status` says nothing plays before a chunk goes out, the queue ran dry: the service logs `deitrix: radio underrun` and that chunk starts the sound again.
- A track with `loop = no` ends after its last chunk: the radio goes off.
- A tick asks for a redraw when the state, the station or the underrun count changed: the end of a track, a read error that stops it, a failed tune restart, an underrun. A tune's replay and a chunk handed over ask for none. The RADIO tab logs `deitrix: radio shows <line>` when a frame shows another station or off.

Pip-OS has one voice, and a UI tone would replace the radio. So while the radio plays, it blocks the UI tones with `dx_ui_sound_block()`. They come back when it stops.

### The autosave

The autosave writes DEITRIX.SAV and INV.SAV when their counters change. It writes 2 s after the last change, and at most 10 s after the first one, so a burst of changes writes once. It writes only the saves that changed. Each write logs `deitrix: save DEITRIX.SAV ok` or `failed`. A failed write is tried again 5 s later. After 3 failed retries it gives up until the next change and logs `deitrix: save gave up`. `dx_autosave_flush()` writes at once; the service's `stop` calls it. A write in QEMU takes 20 to 120 ms, and the UI waits only that long. No screen shows the saves, so the autosave never asks for a redraw.

## Screens

### STAT

`src/screens/stat.c` has three sub-tabs: STATUS, ATTRS and ABOUT. LEFT and RIGHT move between them. At STATUS, LEFT goes to the previous tab, and at ABOUT, RIGHT goes to the next. The sub-tab and the ATTRS selection stay when the tab is left, so a return shows the same view. No view of STAT changes state, so it writes no save.

- STATUS shows the owner's name, the owner's line in dim text under it, then up to 6 gauges in file order. The gauges share one label column and one value column. The name uses the stroke font at unit 4, or unit 3 when 4 does not fit in 312 px. A name that fits neither, or holds a character the stroke font lacks, uses the normal font. `dx_stat_name_unit()` makes that choice. UP, DOWN, SELECT and Back do nothing, and the shell plays the deny tone.
- ATTRS shows up to 12 attributes as a list: the name at the left, the value at the right. Ten rows fit, so 11 or 12 scroll. UP and DOWN move the selection. SELECT does nothing and denies. With no attributes it shows "No attributes in the profile.".
- ABOUT shows the device data as key and value rows: the Deitrix version, the Pip-OS version and ABI, the SoC, the RAM, the uptime and the time in UTC. Under them it shows the profile's about lines in dim text, as many as fit: 3. The keys do nothing there, except LEFT and RIGHT.

When PROFILE.INI is missing, STATUS and ATTRS show "No profile on the card.". When the file was ignored, they show "The profile was not read." and the reason, such as "PROFILE.INI line 12". ABOUT still shows the device data.

The device rows come from `dx_plat_sysinfo()`. "Pip-OS" is `pipos_version` and "ABI" is `abi_version`. "SoC" is `soc` as BCM2835, BCM2836 or BCM2837. "RAM" is `mem_size`: the RAM the firmware gives the ARM, in MiB. The Deitrix version is `VERSION` in the Makefile, the version the SDK seals the ROM with. The Makefile passes it to the code as `DX_VERSION`. `src/model/stat_fmt.c` writes every value as text.

The uptime is `ticks_ms`, shown as `d hh:mm:ss`. Pip-OS has no other uptime source, and `ticks_ms` wraps at 2^31 ms. So after 24 days 20:31:23 the uptime shown starts again at `0d 00:00:00`. The time comes from `get_time`, or reads "no clock".

Only ABOUT ticks. Its tick wakes on each whole second of `ticks_ms` and asks for a redraw when the uptime second or the clock changed. On STATUS or ATTRS the tick returns 0, so the shell stops it. The shell ticks the screen after each event, so a move to ABOUT starts it at once. A frame of ABOUT reads the clock itself, so the time can show up to 1 s late.

The tab logs one line when a frame shows another view, just before the present. The first STATUS frame after the tab opens logs nothing:

```
deitrix: stat shows ATTRS sel=3 top=0
deitrix: stat shows STATUS
deitrix: stat about Deitrix=0.1.0 | Pip-OS=7.1.0 | ABI=1.2 | SoC=BCM2836 | RAM=960 MiB | Uptime=0d 00:00:12 | Time=no clock
```

ABOUT logs its line on each frame, so once a second. `model/profile.h` and `ui/gauge.h` both define `struct dx_gauge`, so `stat.c` cannot include both. `src/screens/stat_draw.c` draws the name and the gauges from plain values for it.

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

The footer says `UP/DOWN: item  SELECT: open` at the list, `LEFT/RIGHT: category  DIAL: tab` at an empty category and `UP/DOWN: count  SELECT: mark  BACK` in the detail. The empty states keep the shell's hint.

Each key the tab takes logs one serial line before the redraw, for the QEMU tests: `deitrix: inv cat=1 row=0`, `open=knife`, `count=3`, `refused count=999`, `marked=1`, `back row=0`, `end row=0` at a list end, and `idle` for a key that changes nothing. LEFT or RIGHT that changes the tab logs no `inv` line.

### MAP

`src/screens/map.c` shows the maps of MAP.INI, one sub-tab per map in file order: LOCAL, then WORLD. A card with one map has one sub-tab. LEFT and RIGHT move between them. At the first sub-tab LEFT goes to DATA, and at the last RIGHT goes to RADIO. No view of MAP changes state, so it writes no save. Nothing ticks: the screen redraws only after a key.

- The map view draws the map's lines in dim, fitted into the pane under the sub-tab row with `dx_map_fit()`. The fit keeps the aspect ratio, centres the map and leaves a margin of 8 px. The pane is the clip rectangle while the map draws. Each place is a 5 px square in fg. The selected place is a reticle: a 9 px ring around a 3 px dot, with a 3 px tick out of each side. The map's title sits at the right of the sub-tab row in dim text. Under a faint rule, the last row shows the selected place's name at the left and its kind at the right in dim text. UP and DOWN cycle the places in file order and wrap at both ends. SELECT opens the detail. With no places the row says "No places on this map.", and UP, DOWN and SELECT deny. With one place UP and DOWN deny too. A map with places and no lines shows the places alone.
- The detail shows the place's name in the title row, then four key rows: the kind, the position as the file gives it, the map's title and the place's number, such as "2 of 7". Under them a locator shows the whole map in a faint frame, the other places dim and the place as the reticle. Back or LEFT returns to the map with the same place selected. RIGHT does nothing and stays in the detail. UP, DOWN and SELECT deny.

The format has no owner position and no unit, so the detail shows no distance. It has no note per place either.

When MAP.INI is missing, ignored or holds no valid map, the body shows "No map on the card." and a dim reason, such as "MAP.INI missing", "MAP.INI line 4" or "MAP.INI holds no map.". The keys do nothing there, and LEFT and RIGHT change the tab.

Leaving the tab closes the detail. The sub-tab and each map's selected place stay.

The hint names the keys that do something now: "UP/DOWN: place  SELECT: open  RIGHT: map" on LOCAL with WORLD next, "BACK/LEFT: back to the map" in the detail. A single map without places and the empty state use the shell's hint.

`src/model/map_view.c` holds the pure helpers: the pane fit with its margin, a line in pixels, the place cycling, the marker squares and the position text. A map point lands on the pixel `dx_map_to_px()` gives: rounded to the nearest, halves up.

The tab logs one line when a frame shows another view, just before the present, and the time the map view took after each map frame. The first frame after the tab opens logs nothing:

```
deitrix: map shows LOCAL place=2
deitrix: map drew 0 ms
deitrix: map detail LOCAL place=2
deitrix: map shows none
```

The time comes from `ticks_ms`, which moves in 10 ms steps in QEMU. With the largest map the format allows, 64 lines of 16 points and 24 places, each redraw of the map view logged 0 ms on raspi0 and raspi2b in QEMU, and under 100 ms on every board.

## Planned

These are plans, not code:

- The four screens with real content: INV, DATA, MAP and RADIO.
- A holotape game.
