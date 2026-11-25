/*
 * The DATA tab's CLOCK sub-tab: the time in large digits, the date and
 * the weekday, refreshed each second. SELECT opens the setter: year,
 * month, day, hour and minute in one row. SELECT there stores the time
 * through Pip-OS's set_time, with the seconds at 0.
 */
#include "model/data_time.h"
#include "model/util.h"
#include "screens/data_view.h"
#include "ui/vfont.h"

#define POLL_MS   250 /* how often the page reads the clock */
#define TIME_UNIT 6   /* the stroke font's unit for HH:MM:SS */
#define TIME_DY   22
#define DATE_DY   72
#define UTC_DY    92
#define FIELD_SCALE 2
#define FIELD_DY  44
#define LABEL_DY  84
#define LINE_SIZE 128

/* Under "Time set.": true with a clock chip and without one. */
#define NO_CHIP "Lost at power-off without a clock chip."

/* The setter row, "YYYY-MM-DD HH:MM": each field's first column and width. */
static const int field_col[DX_DT_FIELDS] = { 0, 5, 8, 11, 14 };
static const int field_len[DX_DT_FIELDS] = { 4, 2, 2, 2, 2 };
static const char *const field_name[DX_DT_FIELDS] = { "Year", "Month", "Day", "Hour", "Minute" };

typedef struct clock_state {
    int open;            /* 1 while the setter is open */
    int field;           /* the active field */
    dx_dt_t edit;        /* the setter's value */
    const char *message; /* the setter's error line, or NULL */
    const char *status;  /* the page's line after a store, or NULL */
    int shown_valid;     /* what the last page frame showed */
    uint32_t shown;
} clock_state_t;

static clock_state_t clk;

void dx_data_clock_reset(void)
{
    dx_mem_set(&clk, 0, sizeof(clk));
}

void dx_data_clock_leave(void)
{
    clk.open = 0;
    clk.message = NULL;
    clk.status = NULL;
}

int dx_data_clock_nested(void)
{
    return clk.open;
}

/* The setter starts from the clock, or 2026-01-01 00:00 without one. */
static void open_setter(void)
{
    static const dx_dt_t start = { 2026, 1, 1, 0, 0, 0 };
    uint32_t now = 0;

    clk.edit = start;
    if (dx_plat_time(&now)) {
        dx_dt_from_unix(now, &clk.edit);
        if (clk.edit.year < DX_DT_YEAR_MIN || clk.edit.year > DX_DT_YEAR_MAX) {
            clk.edit = start;
        }
    }
    clk.edit.second = 0;
    clk.open = 1;
    clk.field = DX_DT_YEAR;
    clk.message = NULL;
    clk.status = NULL;
}

static void store(void)
{
    char line[LINE_SIZE];
    uint32_t s;
    int32_t rc;
    dx_sb_t sb;

    clk.edit.second = 0;
    s = dx_dt_to_unix(&clk.edit);
    rc = dx_plat_set_time(s);
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: data set_time ");
    dx_sb_uint(&sb, s);
    dx_sb_str(&sb, " rc=");
    dx_sb_int(&sb, rc);
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
    if (rc == 0) {
        /* The ABI does not say whether a clock chip took it, so the
         * page says what happens without one. */
        clk.open = 0;
        clk.status = "Time set.";
    } else if (rc == DX_EIO) {
        clk.message = "The clock chip did not take the time.";
    } else {
        clk.message = "The time was not set.";
    }
}

static unsigned setter_event(const dx_ui_event_t *ev)
{
    switch (ev->type) {
    case DX_UI_UP:
    case DX_UI_DOWN:
        dx_dt_step(&clk.edit, clk.field, ev->type == DX_UI_UP ? 1 : -1);
        clk.message = NULL;
        return DX_UI_HANDLED | DX_UI_DIRTY;
    case DX_UI_RIGHT:
        if (clk.field + 1 < DX_DT_FIELDS) {
            clk.field++;
            return DX_UI_HANDLED | DX_UI_DIRTY;
        }
        return DX_UI_HANDLED;
    case DX_UI_LEFT:
        if (clk.field > 0) {
            clk.field--;
            return DX_UI_HANDLED | DX_UI_DIRTY;
        }
        if (ev->repeat) {
            return DX_UI_HANDLED; /* a held LEFT stops at the first field */
        }
        clk.open = 0; /* LEFT on the first field is Back */
        return DX_UI_HANDLED | DX_UI_DIRTY;
    case DX_UI_BACK:
        if (ev->repeat) {
            return DX_UI_IGNORED; /* closes on a fresh press only */
        }
        clk.open = 0;
        return DX_UI_HANDLED | DX_UI_DIRTY;
    case DX_UI_SELECT:
        if (ev->repeat) {
            return DX_UI_IGNORED; /* stores once per press */
        }
        store();
        return DX_UI_HANDLED | DX_UI_DIRTY;
    default:
        return DX_UI_IGNORED;
    }
}

unsigned dx_data_clock_event(const dx_ui_event_t *ev)
{
    if (clk.open) {
        return setter_event(ev);
    }
    if (ev->type == DX_UI_SELECT && !ev->repeat) {
        open_setter();
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    return DX_UI_IGNORED;
}

uint32_t dx_data_clock_tick(uint32_t now_ms, unsigned *flags)
{
    uint32_t now = 0;
    int valid;

    if (clk.open) {
        return 0;
    }
    valid = dx_plat_time(&now);
    if (valid != clk.shown_valid || (valid && now != clk.shown)) {
        *flags |= DX_UI_DIRTY;
    }
    return dx_ticks_add(now_ms, POLL_MS);
}

static void centred(const dx_ctx_t *ctx, dx_rect_t r, int y, const char *s, uint32_t color)
{
    ctx->gfx->text(r.x + (r.w - dx_text_width(s, 1)) / 2, y, s, color, DX_COLOR_NONE, 1);
}

static void draw_page(const dx_ctx_t *ctx, dx_rect_t r)
{
    char line[LINE_SIZE];
    char tm[9], date[11], when[32];
    uint32_t now = 0;
    int valid = dx_plat_time(&now);
    dx_dt_t t;
    dx_sb_t sb;
    int w;

    clk.shown_valid = valid;
    clk.shown = now;
    dx_dt_from_unix(now, &t);
    dx_dt_time_text(valid, &t, tm);
    dx_dt_date_text(&t, date);
    dx_sb_init(&sb, when, sizeof(when));
    dx_sb_str(&sb, date);
    dx_sb_str(&sb, "  ");
    dx_sb_str(&sb, dx_dt_weekday_name(dx_dt_weekday(t.year, t.month, t.day)));

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: data clock ");
    if (valid) {
        dx_sb_str(&sb, tm);
        dx_sb_char(&sb, ' ');
        dx_sb_str(&sb, date);
        dx_sb_char(&sb, ' ');
        dx_sb_str(&sb, dx_dt_weekday_name(dx_dt_weekday(t.year, t.month, t.day)));
    } else {
        dx_sb_str(&sb, "none");
    }
    dx_sb_str(&sb, " | ");
    dx_sb_str(&sb, clk.status != NULL ? clk.status : "");
    dx_sb_char(&sb, '\n');
    dx_data_log(line);

    w = dx_vfont_width(tm, TIME_UNIT);
    dx_vfont_draw(ctx, r.x + (r.w - w) / 2, r.y + TIME_DY, tm, TIME_UNIT,
                  valid ? ctx->theme.fg : ctx->theme.dim);
    if (valid) {
        centred(ctx, r, r.y + DATE_DY, when, ctx->theme.fg);
        centred(ctx, r, r.y + UTC_DY, "The clock runs in UTC.", ctx->theme.dim);
    } else {
        centred(ctx, r, r.y + DATE_DY, "No clock. SELECT sets the time.", ctx->theme.dim);
    }
    if (clk.status != NULL) {
        centred(ctx, r, r.y + r.h - 2 * DX_ROW_H - 2, clk.status, ctx->theme.fg);
        centred(ctx, r, r.y + r.h - DX_ROW_H - 2, NO_CHIP, ctx->theme.dim);
    }
}

static void field_text(int f, char out[5])
{
    static const char digits[] = "0123456789";
    int v = f == DX_DT_YEAR ? clk.edit.year
          : f == DX_DT_MONTH ? clk.edit.month
          : f == DX_DT_DAY ? clk.edit.day
          : f == DX_DT_HOUR ? clk.edit.hour
                            : clk.edit.minute;
    int n = field_len[f];
    int i;

    for (i = n - 1; i >= 0; i--) {
        out[i] = digits[v % 10];
        v /= 10;
    }
    out[n] = '\0';
}

static void draw_setter(const dx_ctx_t *ctx, dx_rect_t r)
{
    static const char *const seps[DX_DT_FIELDS - 1] = { "-", "-", " ", ":" };
    int cw = DX_FONT_W * FIELD_SCALE;
    int x0 = r.x + (r.w - 16 * cw) / 2;
    int y = r.y + FIELD_DY;
    char line[LINE_SIZE];
    char text[5];
    char date[11];
    dx_sb_t sb;
    int f, x;

    dx_dt_date_text(&clk.edit, date);
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: data setter field=");
    dx_sb_int(&sb, clk.field);
    dx_sb_char(&sb, ' ');
    dx_sb_str(&sb, date);
    dx_sb_char(&sb, ' ');
    field_text(DX_DT_HOUR, text);
    dx_sb_str(&sb, text);
    dx_sb_char(&sb, ':');
    field_text(DX_DT_MINUTE, text);
    dx_sb_str(&sb, text);
    dx_sb_str(&sb, " | ");
    dx_sb_str(&sb, clk.message != NULL ? clk.message : "");
    dx_sb_char(&sb, '\n');
    dx_data_log(line);

    centred(ctx, r, r.y + 12, "Set the time, in UTC", ctx->theme.dim);
    for (f = 0; f < DX_DT_FIELDS; f++) {
        x = x0 + field_col[f] * cw;
        field_text(f, text);
        if (f == clk.field) {
            ctx->gfx->rect(x, y - 2, field_len[f] * cw, DX_FONT_H * FIELD_SCALE + 4,
                           ctx->theme.fg, 1);
            ctx->gfx->text(x, y, text, ctx->theme.bg, DX_COLOR_NONE, FIELD_SCALE);
        } else {
            ctx->gfx->text(x, y, text, ctx->theme.fg, DX_COLOR_NONE, FIELD_SCALE);
        }
        if (f + 1 < DX_DT_FIELDS) {
            ctx->gfx->text(x + field_len[f] * cw, y, seps[f], ctx->theme.dim, DX_COLOR_NONE,
                           FIELD_SCALE);
        }
    }
    x = x0 + field_col[clk.field] * cw + field_len[clk.field] * cw / 2;
    x -= dx_text_width(field_name[clk.field], 1) / 2;
    ctx->gfx->text(x, r.y + LABEL_DY, field_name[clk.field], ctx->theme.dim, DX_COLOR_NONE, 1);
    if (clk.message != NULL) {
        centred(ctx, r, r.y + LABEL_DY + 2 * DX_ROW_H, clk.message, ctx->theme.fg);
    }
    centred(ctx, r, r.y + r.h - DX_ROW_H - 2, "SELECT stores it with 0 seconds.", ctx->theme.dim);
}

void dx_data_clock_draw(const dx_ctx_t *ctx, dx_rect_t r)
{
    if (clk.open) {
        draw_setter(ctx, r);
    } else {
        draw_page(ctx, r);
    }
}

const char *dx_data_clock_hint(void)
{
    return clk.open ? "UP/DOWN: set  SELECT: ok  BACK: cancel" : "SELECT: set time";
}
