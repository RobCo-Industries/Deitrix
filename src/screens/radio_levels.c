/*
 * The levels view of the RADIO tab: one gauge for each kind of sound,
 * from ABI 1.6. SELECT on the Volume row opens it. Pip-OS holds the
 * levels: the view reads them when it opens, and each step sets one at
 * once with dx_plat_level_set. The autosave stores them later; the view
 * never writes the card. Each key logs one "deitrix: radio level..."
 * line before the redraw, so the QEMU tests can follow it.
 */
#include "screens/radio_screen.h"

#include "services/services.h"
#include "ui/frame.h"

#define PAD_L   12  /* as the gauge rows' labels */
#define LOG_CAP 80  /* the open line with four levels of 100 is 65 */

/* The rows, in order, with the kind each one sets. */
static const struct {
    uint32_t kind;
    const char *key;   /* in the log lines */
    const char *label;
    const char *note;  /* the line under the rows */
} rows[DX_RADIO_LEVELS_ROWS] = {
    { DX_SOUND_RADIO, "radio", "Radio tunes", "Stations that play a tune" },
    { DX_SOUND_SFX, "sfx", "Effects", "Keys, tapes, tuner and boot sounds" },
    { DX_SOUND_RECORDINGS, "recordings", "Recordings", "Recorded stations and tapes" },
    { DX_SOUND_GAMES, "games", "Games", "Holotape games" },
};

static int is_open;
static int row;
static int32_t levels[DX_RADIO_LEVELS_ROWS];

static void log_text(const char *text)
{
    char line[LOG_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: radio ");
    dx_sb_str(&sb, text);
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
}

/* "deitrix: radio level <what><key><sep><n>". */
static void log_level(const char *what, const char *key, const char *sep, int32_t n)
{
    char line[LOG_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: radio level ");
    dx_sb_str(&sb, what);
    dx_sb_str(&sb, key);
    dx_sb_str(&sb, sep);
    dx_sb_int(&sb, n);
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
}

int dx_radio_levels_there(void)
{
    dx_sysinfo_t si;

    return dx_plat_sysinfo(&si) == 0 && si.abi_version >= DX_ABI_1_6;
}

int dx_radio_levels_is_open(void)
{
    return is_open;
}

unsigned dx_radio_levels_open(void)
{
    char line[LOG_CAP];
    dx_sb_t sb;
    int32_t read[DX_RADIO_LEVELS_ROWS];
    int i;

    if (!dx_radio_levels_there()) {
        return DX_UI_IGNORED;
    }
    for (i = 0; i < DX_RADIO_LEVELS_ROWS; i++) {
        read[i] = dx_plat_level(rows[i].kind);
        if (read[i] < 0) {
            return DX_UI_IGNORED;
        }
    }
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: radio levels");
    for (i = 0; i < DX_RADIO_LEVELS_ROWS; i++) {
        levels[i] = read[i];
        dx_sb_char(&sb, ' ');
        dx_sb_str(&sb, rows[i].key);
        dx_sb_char(&sb, '=');
        dx_sb_int(&sb, read[i]);
    }
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
    is_open = 1;
    row = 0;
    return DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL;
}

void dx_radio_levels_close(void)
{
    is_open = 0;
}

static unsigned move(int step)
{
    int to = row + step;

    if (to < 0 || to >= DX_RADIO_LEVELS_ROWS) {
        log_level("end row=", "", "", row);
        return DX_UI_IGNORED;
    }
    row = to;
    log_level("row=", "", "", row);
    return DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_ROW_MOVED;
}

/* The next multiple of the step up or down, inside 0 to 100. */
static int32_t level_step(int32_t v, int up)
{
    if (up) {
        v = (v / DX_RADIO_LEVEL_STEP + 1) * DX_RADIO_LEVEL_STEP;
    } else {
        v = ((v + DX_RADIO_LEVEL_STEP - 1) / DX_RADIO_LEVEL_STEP - 1) * DX_RADIO_LEVEL_STEP;
    }
    return v < 0 ? 0 : v > DX_VOLUME_MAX ? DX_VOLUME_MAX : v;
}

/* LEFT or RIGHT: one step of the row's level. Always handled, so it
 * never closes the view or changes the tab. A step that changed the
 * level plays the move tone as a play of the row's kind, after the set,
 * so the user hears the new level. */
static unsigned step(const dx_ui_event_t *ev)
{
    int32_t v = level_step(levels[row], ev->type == DX_UI_RIGHT);
    int32_t rc;

    if (v == levels[row]) {
        log_level("refused ", rows[row].key, "=", levels[row]);
        if (!ev->repeat) {
            dx_ui_sound(DX_SND_DENY);
        }
        return DX_UI_HANDLED;
    }
    rc = dx_plat_level_set(rows[row].kind, (uint32_t)v);
    if (rc < 0) {
        log_level("failed ", rows[row].key, " rc=", rc);
        if (!ev->repeat) {
            dx_ui_sound(DX_SND_DENY);
        }
        return DX_UI_HANDLED;
    }
    levels[row] = v;
    dx_autosave_note_levels();
    /* The line goes out before the tone starts, so a test sees the
     * audio trace after it. */
    log_level("", rows[row].key, "=", v);
    dx_ui_sound_as(DX_SND_MOVE, rows[row].kind);
    return DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED;
}

unsigned dx_radio_levels_event(const dx_ui_event_t *ev)
{
    switch (ev->type) {
    case DX_UI_UP:
    case DX_UI_DOWN:
        return move(ev->type == DX_UI_UP ? -1 : 1);
    case DX_UI_LEFT:
    case DX_UI_RIGHT:
        return step(ev);
    case DX_UI_SELECT:
    case DX_UI_BACK:
        /* Not dx_ui_is_back(): LEFT lowers a level here. */
        if (ev->repeat) {
            log_text("idle");
            return DX_UI_HANDLED;
        }
        is_open = 0;
        log_text("levels closed");
        return DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL;
    default:
        log_text("idle");
        return DX_UI_IGNORED;
    }
}

void dx_radio_levels_draw(const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_theme_t *t = &ctx->theme;
    int y = area.y + DX_RADIO_LEVELS_RULE_DY;
    int i;

    ctx->gfx->text(area.x + PAD_L, area.y + DX_RADIO_LEVELS_TITLE_DY, "SOUND LEVELS", t->fg,
                   DX_COLOR_NONE, 1);
    ctx->gfx->line(area.x, y, area.x + area.w - 1, y, t->faint);
    for (i = 0; i < DX_RADIO_LEVELS_ROWS; i++) {
        dx_radio_gauge_row(ctx, area, area.y + DX_RADIO_LEVELS_ROWS_DY + i * DX_ROW_H,
                           rows[i].label, (int)levels[i], i == row);
    }
    ctx->gfx->text(area.x + PAD_L, area.y + DX_RADIO_LEVELS_NOTE_DY, rows[row].note, t->dim,
                   DX_COLOR_NONE, 1);
}
