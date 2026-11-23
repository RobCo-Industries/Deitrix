/*
 * The STAT tab: three sub-tabs. STATUS shows the owner and the gauges,
 * ATTRS the attributes as a list, ABOUT the device data and the about
 * lines. Only ABOUT ticks, once a second.
 */
#include "screens/screens.h"

#include "content.h"
#include "model/stat_fmt.h"
#include "model/util.h"
#include "screens/stat_draw.h"
#include "ui/frame.h"
#include "ui/grid.h"
#include "ui/list.h"
#include "ui/subtabs.h"

enum { SUB_STATUS, SUB_ATTRS, SUB_ABOUT, SUB_COUNT };

static const char *const sub_labels[SUB_COUNT] = { "STATUS", "ATTRS", "ABOUT" };

/* Offsets from the top of the area under the sub-tabs. */
#define NAME_DY   6  /* the name band, NAME_H high */
#define NAME_H    26
#define LINE_DY   38 /* the owner's line */
#define RULE_DY   60 /* a faint rule over the gauges */
#define GAUGE_DY  66
#define GAUGE_PITCH 18
#define LIST_DY   DX_GRID_GAP
#define KV_DY     DX_GRID_GAP
#define KV_ROWS   7
#define ABOUT_GAP 4  /* from the last key row to the rule */

#define LOG_SIZE 192

typedef struct stat_state {
    int ready;
    dx_subtabs_t subs;
    dx_list_t attrs;
    /* ABOUT: what the last frame showed, so the tick asks for a redraw
     * only when it changes. */
    uint32_t shown_s;
    int shown_clock_valid;
    uint32_t shown_clock;
    int sys_read;
    int sys_ok;
    dx_sysinfo_t sys;
    char logged[LOG_SIZE];
    uint32_t logged_min; /* ABOUT: the uptime minute of its last line */
} stat_state_t;

static stat_state_t stat;

static const dx_profile_t *profile(void)
{
    return dx_content_profile();
}

static void attr_row(void *user, int i, dx_row_t *out)
{
    static char vals[DX_ATTRS_MAX][4];
    const dx_profile_t *p = profile();
    dx_sb_t sb;

    (void)user;
    dx_sb_init(&sb, vals[i], sizeof(vals[i]));
    dx_sb_uint(&sb, p->attrs[i].value);
    out->label = p->attrs[i].name;
    out->value = vals[i];
    out->flags = 0;
}

static dx_rect_t below_tabs(dx_rect_t area)
{
    dx_rect_t r = { area.x, area.y + DX_SUBTABS_H, area.w, area.h - DX_SUBTABS_H };

    return r;
}

static void setup(void)
{
    const dx_profile_t *p = profile();
    int count = p->load.status == DX_LOAD_OK ? p->attr_count : 0;
    int h = DX_BODY_H - DX_SUBTABS_H - LIST_DY;

    if (!stat.ready) {
        stat.ready = 1;
        dx_subtabs_init(&stat.subs, sub_labels, SUB_COUNT);
        dx_list_init(&stat.attrs, count, h / DX_ROW_H, attr_row, NULL,
                     "No attributes in the profile.");
    }
    /* The shell enters the first tab before the content loads, so the
     * count is read again each time. */
    if (stat.attrs.count != count) {
        stat.attrs.count = count;
        dx_list_select(&stat.attrs, stat.attrs.sel);
    }
}

void dx_stat_reset(void)
{
    dx_mem_set(&stat, 0, sizeof(stat));
}

static void log_once(const char *line)
{
    if (dx_str_eq(line, stat.logged)) {
        return;
    }
    dx_str_cut(stat.logged, sizeof(stat.logged), line, dx_str_len(line));
    dx_plat_log(line);
}

/* 1 when the last line logged is ABOUT's: the page is still shown. */
static int about_logged(void)
{
    static const char prefix[] = "deitrix: stat about";
    int i;

    for (i = 0; prefix[i] != '\0'; i++) {
        if (stat.logged[i] != prefix[i]) {
            return 0;
        }
    }
    return 1;
}

static void enter(void *st)
{
    (void)st;
    setup();
    stat.logged[0] = '\0';
}

static void leave(void *st)
{
    (void)st;
    stat.logged[0] = '\0';
}

static unsigned event(void *st, const dx_ui_event_t *ev)
{
    unsigned r;

    (void)st;
    setup();
    r = dx_subtabs_event(&stat.subs, ev);
    if (r != DX_UI_IGNORED) {
        return r;
    }
    if (stat.subs.active == SUB_ATTRS) {
        return dx_list_event(&stat.attrs, ev);
    }
    return DX_UI_IGNORED;
}

static uint32_t uptime_s(uint32_t now_ms)
{
    return now_ms / 1000u;
}

static uint32_t tick(void *st, uint32_t now_ms, unsigned *flags)
{
    uint32_t clock = 0;
    int valid;

    (void)st;
    setup();
    if (stat.subs.active != SUB_ABOUT) {
        return 0;
    }
    valid = dx_plat_time(&clock);
    if (uptime_s(now_ms) != stat.shown_s || valid != stat.shown_clock_valid ||
        (valid && clock != stat.shown_clock)) {
        *flags |= DX_UI_DIRTY;
    }
    /* The next whole second of the uptime. */
    return dx_ticks_add(now_ms, 1000u - now_ms % 1000u);
}

/* A centred message in the area, and a dim second line when given. */
static void message(const dx_ctx_t *ctx, dx_rect_t r, const char *a, const char *b)
{
    int y = r.y + (r.h - DX_ROW_H) / 2 - (b != NULL ? DX_ROW_H / 2 : 0);

    ctx->gfx->text(r.x + (r.w - dx_text_width(a, 1)) / 2, y, a, ctx->theme.dim, DX_COLOR_NONE,
                   1);
    if (b != NULL) {
        ctx->gfx->text(r.x + (r.w - dx_text_width(b, 1)) / 2, y + DX_ROW_H, b, ctx->theme.dim,
                       DX_COLOR_NONE, 1);
    }
}

/* The empty state of STATUS and ATTRS when the profile did not load. */
static int no_profile(const dx_ctx_t *ctx, dx_rect_t r)
{
    const dx_profile_t *p = profile();
    char why[40];

    if (p->load.status == DX_LOAD_OK) {
        return 0;
    }
    if (p->load.status == DX_LOAD_MISSING) {
        message(ctx, r, "No profile on the card.", NULL);
    } else {
        message(ctx, r, "The profile was not read.",
                dx_file_reason(&p->load, "PROFILE.INI", why, sizeof(why)));
    }
    return 1;
}

static void draw_status(const dx_ctx_t *ctx, dx_rect_t r)
{
    const dx_profile_t *p = profile();
    const char *labels[DX_GAUGES_MAX];
    int32_t values[DX_GAUGES_MAX];
    int32_t maxes[DX_GAUGES_MAX];
    int i;

    /* The first view after the tab opens logs nothing, so the shell's
     * own lines stay the only ones; a return from another sub-tab logs. */
    if (stat.logged[0] != '\0') {
        log_once("deitrix: stat shows STATUS\n");
    }
    if (no_profile(ctx, r)) {
        return;
    }
    if (p->name[0] != '\0') {
        dx_stat_draw_name(ctx, r, r.y + NAME_DY, NAME_H, p->name);
    }
    if (p->line[0] != '\0') {
        ctx->gfx->text(r.x + (r.w - dx_text_width(p->line, 1)) / 2, r.y + LINE_DY, p->line,
                       ctx->theme.dim, DX_COLOR_NONE, 1);
    }
    if (p->gauge_count == 0) {
        return;
    }
    ctx->gfx->line(r.x + 4, r.y + RULE_DY, r.x + r.w - 5, r.y + RULE_DY, ctx->theme.faint);
    for (i = 0; i < p->gauge_count && i < DX_GAUGES_MAX; i++) {
        labels[i] = p->gauges[i].name;
        values[i] = p->gauges[i].value;
        maxes[i] = p->gauges[i].max;
    }
    dx_stat_draw_gauges(ctx, (dx_rect_t){ r.x, r.y + GAUGE_DY, r.w, r.h - GAUGE_DY },
                        GAUGE_PITCH, labels, values, maxes, i);
}

static void draw_attrs(const dx_ctx_t *ctx, dx_rect_t r)
{
    char line[64];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: stat shows ATTRS sel=");
    dx_sb_int(&sb, stat.attrs.sel);
    dx_sb_str(&sb, " top=");
    dx_sb_int(&sb, stat.attrs.top);
    dx_sb_char(&sb, '\n');
    log_once(line);
    if (no_profile(ctx, r)) {
        return;
    }
    dx_list_draw(&stat.attrs, ctx, (dx_rect_t){ r.x, r.y + LIST_DY, r.w, r.h - LIST_DY });
}

static void draw_about(const dx_ctx_t *ctx, dx_rect_t r)
{
    static const char *const keys[KV_ROWS] = { "Deitrix", "Pip-OS", "ABI", "SoC",
                                               "RAM", "Uptime", "Time" };
    char vals[KV_ROWS][DX_STAT_TEXT_SIZE];
    const dx_profile_t *p = profile();
    char line[LOG_SIZE];
    dx_sb_t sb;
    uint32_t clock = 0;
    int valid = dx_plat_time(&clock);
    int key_cols = 7;
    int vx = r.x + 4 + (key_cols + 2) * DX_FONT_W;
    int y, i;

    if (!stat.sys_read) {
        stat.sys_read = 1;
        stat.sys_ok = dx_plat_sysinfo(&stat.sys) == 0;
    }
    dx_str_cut(vals[0], DX_STAT_TEXT_SIZE, dx_stat_rom_version(),
               dx_str_len(dx_stat_rom_version()));
    if (stat.sys_ok) {
        dx_stat_version3(stat.sys.kernel_version, vals[1]);
        dx_stat_version2(stat.sys.abi_version, vals[2]);
        dx_stat_soc(stat.sys.soc, vals[3]);
        dx_stat_mem(stat.sys.mem_total, vals[4]);
    } else {
        for (i = 1; i <= 4; i++) {
            dx_str_cut(vals[i], DX_STAT_TEXT_SIZE, "unknown", 7);
        }
    }
    dx_stat_uptime(ctx->now_ms, vals[5]);
    dx_stat_datetime(valid, clock, vals[6]);
    stat.shown_s = uptime_s(ctx->now_ms);
    stat.shown_clock_valid = valid;
    stat.shown_clock = clock;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: stat about");
    for (i = 0; i < KV_ROWS; i++) {
        dx_sb_str(&sb, i == 0 ? " " : " | ");
        dx_sb_str(&sb, keys[i]);
        dx_sb_char(&sb, '=');
        dx_sb_str(&sb, vals[i]);
    }
    dx_sb_char(&sb, '\n');
    /* One line when the page shows, then one per uptime minute: enough
     * for the tests, quiet for normal use. */
    if (!about_logged() ||
        stat.shown_s / 60u != stat.logged_min) {
        stat.logged_min = stat.shown_s / 60u;
        log_once(line);
    }

    for (i = 0; i < KV_ROWS; i++) {
        y = r.y + KV_DY + i * DX_ROW_H;
        ctx->gfx->text(r.x + 4, y, keys[i], ctx->theme.dim, DX_COLOR_NONE, 1);
        ctx->gfx->text(vx, y, vals[i], ctx->theme.fg, DX_COLOR_NONE, 1);
    }
    if (p->load.status != DX_LOAD_OK || p->about_count == 0) {
        return;
    }
    y = r.y + KV_DY + KV_ROWS * DX_ROW_H + ABOUT_GAP;
    ctx->gfx->line(r.x + 4, y, r.x + r.w - 5, y, ctx->theme.faint);
    y += 2;
    for (i = 0; i < p->about_count && y + DX_ROW_H <= r.y + r.h; i++, y += DX_ROW_H) {
        ctx->gfx->text(r.x + 4, y, p->about[i], ctx->theme.dim, DX_COLOR_NONE, 1);
    }
}

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    dx_rect_t r = below_tabs(area);

    (void)st;
    setup();
    dx_subtabs_draw(&stat.subs, ctx, area);
    switch (stat.subs.active) {
    case SUB_ATTRS:
        draw_attrs(ctx, r);
        break;
    case SUB_ABOUT:
        draw_about(ctx, r);
        break;
    default:
        draw_status(ctx, r);
        break;
    }
}

static const char *hint(void *st)
{
    (void)st;
    setup();
    switch (stat.subs.active) {
    case SUB_ATTRS:
        return stat.attrs.count > 1 ? "UP/DOWN: move" : DX_HINT_PAGE;
    default:
        return DX_HINT_PAGE;
    }
}

const dx_screen_t dx_screen_stat = {
    "STAT", enter, leave, event, tick, draw, hint, NULL,
};
