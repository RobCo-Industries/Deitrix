/*
 * The RADIO tab: the control panel of the radio service. The stations
 * of RADIO.INI in a list, a block that tells what plays, then two
 * settings: the volume and the UI sounds. UP and DOWN walk one cursor
 * through the stations and then the two settings.
 *
 * Changes go through the service and the settings' setters, which
 * count a change; the autosave writes DEITRIX.SAV. Each key the tab
 * takes logs one "deitrix: radio ..." line before the redraw, so the
 * QEMU tests can follow it.
 */
#include "screens/screens.h"
#include "screens/radio_screen.h"

#include "content.h"
#include "services/services.h"
#include "ui/frame.h"
#include "ui/list.h"

#define PAD_L    12 /* as the list's label */
#define PAD_R    4
#define BAR_X    108
#define BAR_H    10
#define GAP      8
#define LOG_CAP  64 /* "deitrix: radio shows Cannot play ", a name of 20, "\n" */
#define TRACK    "track 1 of 1"
#define MARK     "ON AIR"

static int sel = -1; /* the cursor: a station, then VOLUME, then SOUND; -1 before the first use */

/* 1 once the current key logged its line. */
static int logged;

/* The station the last frame showed: see log_shown. */
static int shown = -1;

void dx_radio_screen_reset(void)
{
    sel = -1;
    shown = -1;
}

static int station_count(const dx_radio_t *r)
{
    return r->load.status == DX_LOAD_OK ? r->count : 0;
}

/* The rows after the stations. */
static int row_volume(const dx_radio_t *r)
{
    return station_count(r);
}

static int row_sound(const dx_radio_t *r)
{
    return station_count(r) + 1;
}

/* Takes the cursor from the last station at the first use, and keeps
 * it inside the rows: the ROM loads once, the host tests load often. */
static void sync(const dx_radio_t *r)
{
    if (sel < 0) {
        int i = dx_radio_find(r, dx_content_settings()->last_station);

        sel = station_count(r) == 0 ? row_volume(r) : i < 0 ? 0 : i;
    }
    if (sel > row_sound(r)) {
        sel = row_volume(r);
    }
}

static void log_line(const char *what, int32_t n)
{
    char line[LOG_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: radio ");
    dx_sb_str(&sb, what);
    if (n >= 0) {
        dx_sb_int(&sb, n);
    }
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
    logged = 1;
}

/* ---- Keys ---------------------------------------------------------------- */

static unsigned move(const dx_radio_t *r, int step)
{
    int to = sel + step;

    if (to < 0 || to > row_sound(r)) {
        log_line("end row=", sel);
        return DX_UI_IGNORED;
    }
    sel = to;
    log_line("row=", sel);
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

/* SELECT on a station: start it, or stop it when it plays. */
static unsigned station_select(const dx_radio_t *r)
{
    const dx_station_t *s = &r->stations[sel];

    if (dx_radio_current() == sel) {
        log_line("off", -1);
        dx_radio_stop();
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    /* The line goes out before the sound starts, so a test sees the
     * audio trace after it. */
    log_line("station=", s->number);
    dx_settings_set_station(dx_content_settings(), s->number);
    if (dx_radio_play(sel, 0) != 0) {
        dx_plat_log("deitrix: radio cannot play\n");
    }
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

/* The next multiple of the step up or down, inside 0 to 100. */
static int volume_step(int v, int up)
{
    if (up) {
        v = (v / DX_RADIO_VOLUME_STEP + 1) * DX_RADIO_VOLUME_STEP;
    } else {
        v = ((v + DX_RADIO_VOLUME_STEP - 1) / DX_RADIO_VOLUME_STEP - 1) * DX_RADIO_VOLUME_STEP;
    }
    return v < 0 ? 0 : v > DX_VOLUME_MAX ? DX_VOLUME_MAX : v;
}

/* LEFT and RIGHT on the Volume row. Always handled, so they never change
 * the tab; at a limit a press plays the deny tone. */
static unsigned volume_key(const dx_ui_event_t *ev)
{
    dx_settings_t *st = dx_content_settings();
    int v = volume_step(st->volume, ev->type == DX_UI_RIGHT);

    if (!dx_settings_set_volume(st, v)) {
        log_line("refused volume=", st->volume);
        if (!ev->repeat) {
            dx_ui_sound(DX_SND_DENY);
        }
        return DX_UI_HANDLED;
    }
    dx_radio_set_volume(v);
    log_line("volume=", v);
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

static unsigned sound_toggle(const dx_ui_event_t *ev)
{
    dx_settings_t *st = dx_content_settings();

    if (ev->repeat) {
        return DX_UI_HANDLED; /* one toggle per press; no tab change */
    }
    dx_settings_set_ui_sound(st, !st->ui_sound);
    /* On first, so the shell's tone for this key plays when it turns on. */
    dx_ui_sound_enable(st->ui_sound);
    log_line("sound=", st->ui_sound);
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

static unsigned event(void *st, const dx_ui_event_t *ev)
{
    const dx_radio_t *r = dx_content_radio();
    int lr = ev->type == DX_UI_LEFT || ev->type == DX_UI_RIGHT;
    unsigned res = DX_UI_IGNORED;

    (void)st;
    logged = 0;
    sync(r);
    if (ev->type == DX_UI_UP || ev->type == DX_UI_DOWN) {
        res = move(r, ev->type == DX_UI_UP ? -1 : 1);
    } else if (sel == row_volume(r)) {
        if (lr) {
            res = volume_key(ev);
        }
    } else if (sel == row_sound(r)) {
        if (lr || (ev->type == DX_UI_SELECT)) {
            res = sound_toggle(ev);
        }
    } else if (ev->type == DX_UI_SELECT && !ev->repeat) {
        res = station_select(r);
    }
    if (!logged && !(res == DX_UI_IGNORED && lr)) {
        /* A key that changed nothing. A test waits for this line to know
         * the frame before it is on screen. LEFT or RIGHT that the shell
         * turns into a tab change logs the tab instead. */
        log_line("idle", -1);
    }
    return res;
}

/* ---- Drawing ------------------------------------------------------------- */

static void station_row(void *user, int index, dx_row_t *out)
{
    const dx_radio_t *r = user;
    const dx_station_t *s = &r->stations[index];

    out->label = s->name;
    out->value = dx_radio_current() == index ? MARK : NULL;
    out->flags = s->playable ? 0u : DX_ROW_DISABLED;
}

/* The file name without its extension: "HEARTH" for HEARTH.PCM. */
static void file_stem(const char *file, char *out, int cap)
{
    int i;

    for (i = 0; i < cap - 1 && file[i] != '\0' && file[i] != '.'; i++) {
        out[i] = file[i];
    }
    out[i] = '\0';
}

/* The station the last frame showed: its index, -1 for off, -2 - i for
 * station i that could not play. Off at start, so a first frame that
 * shows off logs nothing. A frame that shows another logs
 * "deitrix: radio shows <line>", so a target test can read what the tab
 * shows. The line goes out just before the present. */

static void log_shown(int key, const char *what, const char *name)
{
    char line[LOG_CAP];
    dx_sb_t sb;

    if (key == shown) {
        return;
    }
    shown = key;
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: radio shows ");
    dx_sb_str(&sb, what);
    dx_sb_str(&sb, name);
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
}

static void draw_now(const dx_radio_t *r, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_theme_t *t = &ctx->theme;
    int y = area.y + DX_RADIO_NOW_DY;
    int right = area.x + area.w - PAD_R;
    int cur = dx_radio_current();
    int bad = dx_radio_failed();
    char a[44];
    dx_sb_t sb;

    if (cur >= 0 && cur < station_count(r)) {
        char stem[DX_NAME83_SIZE];

        dx_sb_init(&sb, a, sizeof(a));
        dx_sb_str(&sb, "Playing ");
        dx_sb_str(&sb, r->stations[cur].name);
        ctx->gfx->text(area.x + PAD_L, y, a, t->fg, DX_COLOR_NONE, 1);
        file_stem(r->stations[cur].file, stem, sizeof(stem));
        ctx->gfx->text(area.x + PAD_L, y + DX_ROW_H, stem, t->dim, DX_COLOR_NONE, 1);
        ctx->gfx->text(right - dx_text_width(TRACK, 1), y + DX_ROW_H, TRACK, t->dim,
                       DX_COLOR_NONE, 1);
        log_shown(cur, "Playing ", r->stations[cur].name);
    } else if (bad >= 0 && bad < station_count(r)) {
        const dx_station_t *s = &r->stations[bad];

        ctx->gfx->text(area.x + PAD_L, y, "Station has nothing to play.", t->fg, DX_COLOR_NONE,
                       1);
        if (s->file_load.status != DX_LOAD_OK) {
            dx_placeholder_reason(&s->file_load, s->file, a, sizeof(a));
        } else {
            dx_sb_init(&sb, a, sizeof(a));
            dx_sb_str(&sb, s->file);
            dx_sb_str(&sb, " did not play");
        }
        ctx->gfx->text(area.x + PAD_L, y + DX_ROW_H, a, t->dim, DX_COLOR_NONE, 1);
        log_shown(-2 - bad, "Cannot play ", s->name);
    } else {
        ctx->gfx->text(area.x + PAD_L, y, "Radio off.", t->dim, DX_COLOR_NONE, 1);
        log_shown(-1, "Off", "");
    }
}

/* One setting row: the label, an optional bar for `level` of 100, the
 * value at the right. Inverse when selected. */
static void setting_row(const dx_ctx_t *ctx, dx_rect_t area, int y, const char *label,
                        int level, const char *value, int selected)
{
    const dx_theme_t *t = &ctx->theme;
    uint32_t fg = t->fg;
    uint32_t bg = DX_COLOR_NONE;
    int right = area.x + area.w - PAD_R;
    int vx = right - dx_text_width(value, 1);

    if (selected) {
        ctx->gfx->rect(area.x, y, area.w, DX_ROW_H, t->fg, 1);
        fg = t->bg;
        bg = t->fg;
    }
    ctx->gfx->text(area.x + PAD_L, y, label, fg, bg, 1);
    if (level >= 0) {
        /* The bar ends where a value of three digits would start. */
        int bx = area.x + BAR_X;
        int bw = right - 3 * DX_FONT_W - GAP - bx;
        int by = y + (DX_ROW_H - BAR_H) / 2;
        int fill = level * (bw - 4) / DX_VOLUME_MAX;

        ctx->gfx->rect(bx, by, bw, BAR_H, selected ? t->bg : t->dim, 0);
        if (fill > 0) {
            ctx->gfx->rect(bx + 2, by + 2, fill, BAR_H - 4, fg, 1);
        }
    }
    ctx->gfx->text(vx, y, value, fg, bg, 1);
}

static void draw_empty(const dx_radio_t *r, const dx_ctx_t *ctx, dx_rect_t area)
{
    char reason[40];
    int y = area.y + DX_RADIO_LIST_DY + (DX_RADIO_LIST_ROWS * DX_ROW_H - DX_ROW_H) / 2;

    dx_text_centered(ctx, y - DX_ROW_H / 2, "No stations on the card.", ctx->theme.fg, 1);
    if (r->load.status != DX_LOAD_OK) {
        dx_placeholder_reason(&r->load, "RADIO.INI", reason, sizeof(reason));
        dx_text_centered(ctx, y + DX_ROW_H, reason, ctx->theme.dim, 1);
    }
}

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_radio_t *r = dx_content_radio();
    const dx_settings_t *set = dx_content_settings();
    char num[12];
    dx_list_t l;
    int y;

    (void)st;
    sync(r);
    if (station_count(r) == 0) {
        draw_empty(r, ctx, area);
    } else {
        dx_list_init(&l, station_count(r), DX_RADIO_LIST_ROWS, station_row, (void *)r, NULL);
        l.sel = sel < station_count(r) ? sel : -1; /* no row inverse on a setting */
        dx_list_draw(&l, ctx,
                     (dx_rect_t){ area.x, area.y + DX_RADIO_LIST_DY, area.w,
                                  DX_RADIO_LIST_ROWS * DX_ROW_H });
    }
    y = area.y + DX_RADIO_RULE1_DY;
    ctx->gfx->line(area.x, y, area.x + area.w - 1, y, ctx->theme.faint);
    draw_now(r, ctx, area);
    y = area.y + DX_RADIO_RULE2_DY;
    ctx->gfx->line(area.x, y, area.x + area.w - 1, y, ctx->theme.faint);
    dx_itoa(set->volume, num);
    y = area.y + DX_RADIO_SET_DY;
    setting_row(ctx, area, y, "Volume", set->volume, num, sel == row_volume(r));
    setting_row(ctx, area, y + DX_ROW_H, "UI sounds", -1, set->ui_sound ? "On" : "Off",
                sel == row_sound(r));
}

static const char *hint(void *st)
{
    const dx_radio_t *r = dx_content_radio();

    (void)st;
    sync(r);
    if (sel == row_volume(r)) {
        return "LEFT/RIGHT: volume  UP/DOWN: move";
    }
    if (sel == row_sound(r)) {
        return "SELECT: UI sounds on/off  UP/DOWN: move";
    }
    if (dx_radio_current() == sel) {
        return "UP/DOWN: station  SELECT: stop";
    }
    return "UP/DOWN: station  SELECT: play";
}

const dx_screen_t dx_screen_radio = {
    "RADIO", NULL, NULL, event, NULL, draw, hint, NULL,
};
