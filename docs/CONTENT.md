# Content files

Deitrix reads its content from the SD card and keeps changes in saves. This document is the contract for both. `tools/check_content.py` checks every rule below that a file can break. The ROM code that reads these files does not exist yet. It must follow this document.

## Contents

- [Where the files live](#where-the-files-live)
- [Rules for every file](#rules-for-every-file)
- [PROFILE.INI: the STAT tab](#profileini-the-stat-tab)
- [ITEMS.INI: the INV tab](#itemsini-the-inv-tab)
- [NOTES: the DATA tab](#notes-the-data-tab)
- [MAP.INI: the MAP tab](#mapini-the-map-tab)
- [RADIO.INI: the RADIO tab](#radioini-the-radio-tab)
- [Tunes: .TUN files](#tunes-tun-files)
- [PCM tracks: .PCM files](#pcm-tracks-pcm-files)
- [Saves](#saves)
- [Tools](#tools)
- [Making your own content](#making-your-own-content)

## Where the files live

All content lives in `/PIPBOY/DATA/` on the card. In this repository the source is `content/PIPBOY/DATA/`. `make` builds the card tree `build/card/`: a copy of `content/` plus the two PCM tracks that `tools/mkpcm.py` generates. `make sdimage` and `make bootdir` copy that tree to the card root.

| Path on the card | Tab | Limit |
|---|---|---|
| `/PIPBOY/DATA/PROFILE.INI` | STAT | 4096 bytes |
| `/PIPBOY/DATA/ITEMS.INI` | INV | 32768 bytes |
| `/PIPBOY/DATA/NOTES/*.TXT` | DATA | 32 files, 4096 bytes each |
| `/PIPBOY/DATA/MAP.INI` | MAP | 32768 bytes |
| `/PIPBOY/DATA/RADIO.INI` | RADIO | 2048 bytes |
| `/PIPBOY/DATA/RADIO/*.TUN` | RADIO | 4096 bytes each |
| `/PIPBOY/DATA/RADIO/*.PCM` | RADIO | 8000 to 2097152 bytes each |
| `/PIPBOY/SAVE/DEITRIX.SAV` | all | 20 bytes, written by the ROM |
| `/PIPBOY/SAVE/INV.SAV` | INV | 1036 bytes at most, written by the ROM |

Every file except a PCM track fits in a 32 KiB buffer, so the ROM reads each one whole into a fixed buffer. A file above its limit is not read: the tab shows its empty state.

## Rules for every file

### Names

- Every file and folder name under `/PIPBOY/DATA/` is an upper-case 8.3 name: 1 to 8 characters from `A-Z`, `0-9` and `_`, then an optional dot and 1 to 3 of the same characters. This is narrower than what Pip-OS accepts, so it holds on any card.
- Pip-OS upper-cases paths, but `dir_list` returns the name on the card. Keep names upper case on the card too.
- The ROM ignores any other file in `/PIPBOY/DATA/`. The checker reports it as `unknown file`.

### Missing and broken files

- A missing file is not an error. Its tab shows its empty state.
- A file that is too large, that does not parse, or that has no valid version is ignored as a whole. Its tab shows its empty state with the file name and the line of the first error, for example `ITEMS.INI line 12`.
- A file that parses but has a bad entry keeps its good entries. Each section below says what happens to a bad value: it is clamped, cut, set to its default, or its entry is dropped.

### Text

- Text is ASCII: bytes 0x20 to 0x7E, tab, and LF line ends. A CR right before an LF is accepted, so CRLF files work. Any other byte makes the checker fail. In an INI file the ROM rejects control bytes like `ini.c` does, and shows a byte above 0x7E as a box.
- Lengths count characters, which are bytes here.
- A text longer than its limit is cut to the limit by the ROM.

### INI files

PROFILE.INI, ITEMS.INI, MAP.INI and RADIO.INI use the INI syntax of RobCo-Core's `ini.c`, which the Pip-OS SDK ships:

- One item per line: `[section]`, `key = value`, a comment, or a blank line.
- A comment starts with `#` or `;`, on its own line or after a value. Inside an unquoted value, `#` or `;` starts a comment only at the start of the value or after a space or tab, so `a#b` keeps its `#`.
- Section names and keys use `A-Z a-z 0-9 _ . -`. Keys are case-sensitive. Spaces around names and values are dropped.
- A value in double quotes may hold any text, with the escapes `\"`, `\\` and `\n`. Use quotes for a value that holds ` #` or ` ;`.
- Limits from `ini.c`: 255 bytes per line without the line end, 31 per section name, 31 per key, 127 per value after escapes. A longer line, name, key or value is a parse error.
- Keys before the first section belong to the section `""`. The only one allowed there is `version`.
- The ROM reads keys in file order. When a key repeats in a section, the last one wins. When a section repeats, its keys merge. The checker reports both as errors.
- Unknown sections and keys are ignored by the ROM. The checker reports them, so typos show up.

### The version key

Every INI file starts with `version = 1` before its first section. A missing version, or any other value, makes the ROM ignore the file. A `.TUN` file starts with the line `version 1`. A note is plain text and has no version: a new note format would use a new folder or extension.

### Numbers

An integer is 1 to 6 decimal digits: no sign, no `0x`, no spaces inside. In a list such as `82,100,Condition`, fields are split on commas and spaces around each field are dropped. A name is always the last field, so it may hold commas.

### Numbered keys

Some sections use numbered keys: `g1`, `a7`, `l12`, `p3`. N is 1 to the section's limit, without a leading zero. The ROM shows entries in ascending N. Gaps are allowed. A key with N above the limit is an unknown key and is ignored.

## PROFILE.INI: the STAT tab

The owner's profile: a name, gauges, attributes and an about text. The ROM adds the device data itself: kernel version, ABI, memory, uptime and clock. Do not put device data in this file.

| Section | Key | Value | Limit | Bad value |
|---|---|---|---|---|
| `[owner]` | `name` | text | 1 to 24 characters | Empty: the name row is not shown. Longer: cut. |
| `[owner]` | `line` | text, a free line under the name | 0 to 38 characters | Longer: cut |
| `[gauges]` | `g1` to `g6` | `value,max,Name` | `max` 1 to 9999, `value` 0 to `max`, `Name` 1 to 16 characters | `max` not a number or 0: gauge dropped. `value` not a number: gauge dropped. `value` above `max`: clamped to `max`. Name empty: gauge dropped. Longer name: cut. |
| `[attrs]` | `a1` to `a12` | `value,Name` | `value` 0 to 99, `Name` 1 to 16 characters | `value` not a number: dropped. Above 99: clamped to 99. Name empty: dropped. Longer: cut. |
| `[about]` | `l1` to `l8` | text, one line each | 0 to 38 characters | Longer: cut. An empty line shows as a blank row. |

The gauge's name is its label. The ROM draws the bar from `value` and `max` and may show `value/max` beside it. An empty profile, with no owner, gauge or attribute, still shows the device data.

```ini
version = 1

[owner]
name = Field kit 7
line = Carried on walks, kept in the workshop

[gauges]
g1 = 82,100,Condition
g2 = 3,4,Water

[attrs]
a1 = 6,Patience
a2 = 5,Balance

[about]
l1 = This kit holds notes, a small map,
l2 = a list of what is in the bag
```

## ITEMS.INI: the INV tab

The items in the kit, in up to 5 categories. Each category is a sub-tab.

### `[categories]`

One key per category, in the order of the sub-tabs: `<key> = <LABEL>`.

| Part | Rule | Bad value |
|---|---|---|
| key | 1 to 12 of `a-z`, `0-9`, `_` | Category dropped |
| label | 1 to 5 of `A-Z`, shown on the sub-tab | Category dropped, with its items |
| count | At most 5 | The 6th and later are ignored |

### `[item.<id>]`

One section per item, in the order of the list. `<id>` is 1 to 12 of `a-z`, `0-9`, `_`. The id keys the save, so keep it the same when you edit the item. A section with a bad id is ignored. The 65th item and later are ignored.

| Key | Value | Limit | Missing | Bad value |
|---|---|---|---|---|
| `name` | text | 1 to 30 characters | Item dropped | Empty: dropped. Longer: cut. |
| `cat` | a category key | defined in `[categories]` | Item dropped | Unknown category: item dropped |
| `count` | integer | 0 to 999 | 1 | Not a number: 1. Above 999: clamped to 999. |
| `unit` | text after the count, such as `kg` or `pair` | 1 to 6 characters | No unit | Longer: cut |
| `weight` | integer, grams | 0 to 999999 | No weight | Not a number or above: no weight |
| `note` | text for the detail view | 1 to 127 characters | No note | Longer is a parse error |
| `note2` | the rest of the note | 1 to 127 characters | Note is `note` alone | Without `note`: ignored |

The detail view shows `note`, a space, then `note2`, cut to 200 characters. Two keys exist because `ini.c` takes 127 bytes per value. The list row shows the name and, right-aligned, the count and unit, such as `20 m`. The ROM keeps changed counts and a marked flag per item in [INV.SAV](#invsav).

```ini
version = 1

[categories]
tools = TOOLS
food = FOOD

[item.knife]
name = Folding knife
cat = tools
count = 1
weight = 90
note = A small knife with a wooden handle. Keep it dry and oil the hinge
note2 = once a month.

[item.water]
name = Water bottle
cat = food
count = 2
unit = l
```

## NOTES: the DATA tab

Plain text files in `/PIPBOY/DATA/NOTES/`.

- Each file is `<NAME>.TXT`. Any other file in the folder is ignored.
- The list is in name order: the byte order of the 8.3 names. Name your files `WALK01.TXT`, `WALK02.TXT` and so on to control it.
- At most 32 notes. The 33rd in name order and later are not shown.
- At most 4096 bytes per file. A larger file is not shown.
- The first line is the title, shown in the list: 1 to 30 characters, not only spaces. A longer title is cut. A note with an empty title is not shown.
- The rest of the file is the body. The ROM turns CRLF into LF, then shows the body in the text widget, wrapped at 38 columns by `dx_wrap` in `src/ui/text.c`: breaks on spaces, `\n` forces a break, a tab counts as one space, a word longer than a line is split.
- The body wraps to at most 128 lines, the text widget's `DX_TEXT_MAX_LINES`. A blank line counts as a line. The widget cuts a longer body and shows `[text cut]`. The checker counts lines with the same rules as `dx_wrap`.

```text
Walk to the spring

Left the house after breakfast and took the lane west. The morning
was cool and the grass was wet up to the ankle.

The spring runs well this week. I filled both bottles and sat on the
flat stone for a while.
```

A paragraph can be one long line: the ROM wraps it. The example above wraps by hand only to fit this page.

## MAP.INI: the MAP tab

One or two maps: `local`, and an optional `world`. Each map is drawn from polylines and place markers in integer map units. The ROM scales the map's box to the body area and keeps its aspect ratio. y grows downwards.

### `[map.local]` and `[map.world]`

| Key | Value | Limit | Missing | Bad value |
|---|---|---|---|---|
| `title` | text | 1 to 24 characters | The map's name, `local` or `world` | Longer: cut |
| `box` | `x0,y0,x1,y1` | each 0 to 999, `x0 < x1`, `y0 < y1` | Map dropped | Map dropped |

A file with neither map is an empty state. With both, the MAP tab shows `local` first.

### `[map.<name>.lines]`

`l<N> = x,y x,y x,y ...`, N from 1 to 64. A line is 2 to 16 points separated by one or more spaces. Each point is `x,y` with no space inside, each 0 to 999 and inside the map's box, edges included. A closed shape such as a building repeats its first point at the end. 16 points of `999,999` take 127 characters, the value limit. A line with a bad point, a point outside the box, or the wrong number of points is dropped.

### `[map.<name>.places]`

`p<N> = x,y,kind,Name`, N from 1 to 24. `x,y` is inside the box. `Name` is 1 to 24 characters, cut when longer. `kind` picks the marker:

| Kind | Meaning |
|---|---|
| `home` | Where the owner lives |
| `water` | A spring, a pond, a tap |
| `shelter` | A building or a roof |
| `marker` | Any other point of interest |
| `hazard` | A place to be careful |

A place with a bad point, a point outside the box, an unknown kind or an empty name is dropped. Lines and places sections of a map without its `[map.<name>]` section are ignored.

```ini
version = 1

[map.local]
title = Alder Brook
box = 0,0,400,300

[map.local.lines]
l1 = 0,40 60,60 120,100 170,150 200,210 260,250 330,270 400,280
l5 = 260,130 300,130 300,160 260,160 260,130

[map.local.places]
p1 = 280,145,home,House
p5 = 70,62,water,Spring
p7 = 150,120,hazard,Steep bank
```

## RADIO.INI: the RADIO tab

Up to 6 stations, `[station.1]` to `[station.6]`, listed in ascending number. A station plays a tune through `play_seq` or a PCM track through `sample_queue`. The files live in `/PIPBOY/DATA/RADIO/`.

| Key | Value | Missing | Bad value |
|---|---|---|---|
| `name` | text, 1 to 20 characters | Station dropped | Empty: dropped. Longer: cut. |
| `kind` | `tune` or `pcm` | Station dropped | Station dropped |
| `file` | an upper-case 8.3 name in `RADIO/`: `<NAME>.TUN` for `tune`, `<NAME>.PCM` for `pcm` | Station dropped | Wrong extension: dropped |
| `loop` | `yes` or `no`, for `pcm` only | `yes` | Any other value: `yes`. On a tune: ignored. |

- A station whose file is missing, too large or broken stays in the list, drawn dim, and does not play.
- A tune plays from its start again when it ends, until the owner picks another station or stops it.
- A PCM track with `loop = yes` starts again from its first byte with no gap. With `loop = no` it plays once.
- A file in `RADIO/` that no station names is ignored. The checker reports it.

```ini
version = 1

[station.1]
name = Morning
kind = tune
file = MORNING.TUN

[station.4]
name = Hearth
kind = pcm
file = HEARTH.PCM
loop = yes
```

## Tunes: .TUN files

A tune is a text file that expands one to one into the `pip_note_t` array of `play_seq`: each note or rest becomes one entry of `freq_hz` and `duration_ms`.

### Syntax

- At most 4096 bytes and 127 bytes per line, ASCII as above.
- `;` starts a comment that runs to the end of the line. `#` is a sharp here, not a comment.
- Tokens are separated by spaces or tabs. A line may hold any number of tokens.
- The first line that is not blank or a comment is exactly `version 1`.

| Token | Meaning |
|---|---|
| `tempo <bpm>` | Quarter notes per minute, 30 to 300. Applies to the notes after it. Default 120. |
| `length <L>` | The length of a note without `/L`: 1, 2, 4, 8, 16 or 32. Default 4, a quarter note. |
| `<note>[/L][.]` | A note. `<note>` is a letter `A` to `G`, an optional `#` or `b`, and an octave digit 1 to 8: `C4`, `F#3`, `Bb5`. `/L` sets this note's length. A trailing `.` makes it dotted, one and a half times as long, and needs `/L`. |
| `<hz>hz[/L][.]` | A note at a frequency in Hz, 20 to 20000: `440hz/8`. |
| `R[/L][.]` | A rest. It becomes a note with `freq_hz` 0. |
| `\|:` | Starts the repeated part. |
| `:\|` | Ends the repeated part: the notes since `\|:`, or since the start without one, play once more. |

One `|:`, `:|` pair per tune, not nested. A `|:` needs its `:|`. The repeat must hold at least one note.

### Frequencies and durations

The ROM computes both with integers. The tools do the same.

- Octave 8, from C to B, in Hz: 4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902. For octave `o`, with `s = 8 - o`: `freq = (f8 + (1 << (s - 1))) >> s`, or `f8` when `s` is 0. So `A4` is 440 and `C4` is 262.
- `b` and `#` move one step and may cross the octave: `Cb5` is `B4`, `B#4` is `C5`. The result must stay within C1 to B8.
- `duration_ms = 240000 * k / (tempo * L * 2)`, integer division, with `k` = 2, or 3 for a dotted note.

### Limits

- 1 to 256 notes after the repeat is expanded: one `play_seq` call.
- Each note: `freq_hz` 0 or 20 to 20000, `duration_ms` 1 to 60000, within `play_seq`'s rules.
- The whole tune lasts at most 120000 ms.

A tune that breaks any rule does not play. The ROM checks it once when it loads the file.

```text
version 1
; Morning: a bright tune in G major, 4/4.
tempo 100
|:
G4/8 B4/8 D5/4 B4/4 G4/4
A4/8 B4/8 C5/4 B4/4 A4/4
G4/8 A4/8 B4/4 D5/4 E5/4
D5/2. R/4
:|
E5/4 D5/4 C5/4 B4/4
A4/4 C5/4 B4/4 A4/4
G4/8 A4/8 B4/4 A4/4 F#4/4
G4/2. R/4
```

Two equal notes in a row sound as one long note, since `play_seq` leaves no gap. Put a short rest between them.

## PCM tracks: .PCM files

- Raw 8-bit unsigned mono samples at 8000 Hz. 128 is silence. No header.
- 8000 to 2097152 bytes: 1 second to 262 seconds.
- The ROM streams the file in chunks of at most 65536 bytes: it plays the first with `sample`, then reads the next into a second buffer and hands it to `sample_queue` while the first plays.
- A track with `loop = yes` must end where it starts: the last byte and the first byte differ by at most 16. Then the joint sounds like any other pair of samples, with no click.

The sample tracks `HEARTH.PCM` and `DRIFT.PCM` are 256000 bytes each, 32 seconds. They are not in git: `make` builds them into `build/card/PIPBOY/DATA/RADIO/`.

## Saves

The ROM writes two saves with `write_save` and reads them with `read_save`. Both are little-endian with fixed-width fields. Each starts with the same 12-byte header:

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 4 | `magic` | ASCII, per save |
| 4 | 2 | `version` | 1 |
| 6 | 2 | `count` | Number of payload records |
| 8 | 4 | `crc` | CRC-32 of the payload: the bytes from offset 12 to the end |
| 12 | | payload | `count` records |

The CRC-32 is the common one: polynomial 0xEDB88320 reflected, start 0xFFFFFFFF, final XOR 0xFFFFFFFF, as Python's `zlib.crc32`. The CRC of no bytes is 0.

### DEITRIX.SAV

The settings. `magic` is `DXST`. `count` is 1: one 8-byte record. Total size 20 bytes.

| Offset | Size | Field | Range | Default |
|---|---|---|---|---|
| 12 | 1 | `last_tab` | 0 STAT, 1 INV, 2 DATA, 3 MAP, 4 RADIO | 0 |
| 13 | 1 | `volume` | 0 to 100, the level for `volume` | The level Pip-OS had at ROM start |
| 14 | 1 | `ui_sound` | 0 off, 1 on | 1 |
| 15 | 1 | `last_station` | 1 to 6, the station number, or 0 for none | 0 |
| 16 | 4 | `reserved` | 0 | 0 |

### INV.SAV

The inventory changes. `magic` is `DXIV`. `count` is 0 to 64 records of 16 bytes. Total size `12 + 16 * count`, at most 1036 bytes.

| Offset in the record | Size | Field | Range |
|---|---|---|---|
| 0 | 12 | `id` | The item id, ASCII, padded with NUL bytes. A 12-character id has no NUL. |
| 12 | 2 | `count` | 0 to 999 |
| 14 | 1 | `flags` | Bit 0: marked. Other bits 0. |
| 15 | 1 | `reserved` | 0 |

The ROM writes one record per item in ITEMS.INI order, for every item.

### Reading a save

The ROM never crashes on a save. It reads at most the maximum size, then:

| Case | What the ROM does |
|---|---|
| No save | Uses the content defaults |
| Wrong `magic`, `version` not 1, or `crc` wrong | Ignores the whole save and uses the defaults |
| Size not `12 + record size * count`, or `count` above its limit | Ignores the whole save |
| A field out of range | Uses that field's default; the rest of the save stands |
| INV.SAV record with an id not in ITEMS.INI | Drops the record. The next write leaves it out. |
| Item in ITEMS.INI with no record | Uses the count from ITEMS.INI, not marked |
| Two records with the same id | The first wins |

A save is 1036 bytes at most, well below the 64 KiB limit of Pip-OS. The ROM writes a save only when a value changes, not on every key.

## Tools

| Command | What it does |
|---|---|
| `tools/check_content.py <dir>` | Checks the tree at `<dir>`, which holds `PIPBOY/DATA`. Prints `<path>:<line>: <message>` per problem, line 0 for the whole file, and exits 1. Exits 0 with no output when the tree is valid. |
| `make check-content` | Runs the checker on `build/card/`. `make test-host` runs it. |
| `tools/tun2notes.py <file.TUN>` | Prints the notes the ROM sends to `play_seq`, one `freq_hz duration_ms` per line. `--wav <out.wav>` renders them with a square wave. |
| `tools/mkpcm.py <dir>` | Writes `HEARTH.PCM` and `DRIFT.PCM` into `<dir>`. `--wav` also writes WAV files. `--only <NAME>` writes one track. |

Run the checker on `content/` alone and it reports the two PCM files as missing, since `make` generates them.

## Making your own content

1. Copy `content/PIPBOY/DATA/` to the card's `/PIPBOY/DATA/`, or start with an empty folder. Every file is optional.
2. Edit the INI files in a text editor. Keep `version = 1` at the top. Keep the item ids when you change an item, so your saved counts stay.
3. Write notes as `NOTES/<NAME>.TXT`, title on the first line. Use upper-case 8.3 names.
4. For music, write a `.TUN` file in `RADIO/` and add a station to `RADIO.INI`. `tools/tun2notes.py MY.TUN --wav my.wav` lets you hear it on a computer. For a recording, convert it to raw 8-bit unsigned mono at 8000 Hz, for example `sox in.wav -r 8000 -c 1 -b 8 -e unsigned OUT.PCM`.
5. Check the folder: put it at `<dir>/PIPBOY/DATA` and run `python3 tools/check_content.py <dir>`. Fix each line it prints.
6. To start over, delete `/PIPBOY/SAVE/DEITRIX.SAV` and `INV.SAV` from the card on a computer.
