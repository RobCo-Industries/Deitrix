/*
 * The widget gallery: a test-only ROM with one screen per widget and
 * fixed sample data. It replaces src/screens/ and runs on the same app
 * shell. It logs "gallery: ..." lines the QEMU tests wait for.
 */
#include "platform/platform.h"
#include "screens/screens.h"
#include "ui/frame.h"
#include "ui/gauge.h"
#include "ui/icon.h"
#include "ui/kv.h"
#include "ui/list.h"
#include "ui/subtabs.h"
#include "ui/text.h"
#include "ui/vfont.h"

#define CAPTION_H 24
#define LOG_MAX   64

/* Logs "gallery: <what>=<a>" and, when b >= 0, " <bname>=<b>". */
static void log2(const char *what, int a, const char *bname, int b)
{
    char line[LOG_MAX];
    char num[12];
    int n = 0;
    const char *p;

    for (p = "gallery: "; *p != '\0'; p++) {
        line[n++] = *p;
    }
    for (p = what; *p != '\0' && n < LOG_MAX - 30; p++) {
        line[n++] = *p;
    }
    line[n++] = '=';
    dx_itoa(a, num);
    for (p = num; *p != '\0'; p++) {
        line[n++] = *p;
    }
    if (b >= 0) {
        line[n++] = ' ';
        for (p = bname; *p != '\0' && n < LOG_MAX - 14; p++) {
            line[n++] = *p;
        }
        line[n++] = '=';
        dx_itoa(b, num);
        for (p = num; *p != '\0'; p++) {
            line[n++] = *p;
        }
    }
    line[n++] = '\n';
    line[n] = '\0';
    dx_plat_log(line);
}

static void caption(const dx_ctx_t *ctx, dx_rect_t area, const char *text)
{
    ctx->gfx->text(area.x + 4, area.y + 2, text, ctx->theme.dim, DX_COLOR_NONE, 1);
}

static dx_rect_t below_caption(dx_rect_t area)
{
    area.y += CAPTION_H;
    area.h -= CAPTION_H;
    return area;
}

/* ---- LIST --------------------------------------------------------------- */

#define LIST_ROWS 8

static const dx_row_t supplies[] = {
    { "Water filter", "3", 0 },
    { "Canned beans", "12", 0 },
    { "Wind-up radio", "1", DX_ROW_MARKED },
    { "Wool blanket", "2", 0 },
    { "First aid kit", "1", 0 },
    { "Spare batteries for the lantern and the torch", "16", 0 },
    { "Rope, 20 m", "1", 0 },
    { "Seed packets", "9", DX_ROW_MARKED },
    { "Broken compass", "1", DX_ROW_DISABLED },
    { "Candles", "24", 0 },
    { "Notebook", "2", 0 },
    { "Pencil stubs", "5", 0 },
};

static dx_list_t list;

static void supply_row(void *user, int i, dx_row_t *out)
{
    (void)user;
    *out = supplies[i];
}

static void list_enter(void *st)
{
    (void)st;
    if (list.row == NULL) {
        dx_list_init(&list, sizeof(supplies) / sizeof(supplies[0]), LIST_ROWS, supply_row, NULL,
                     "No supplies.");
    }
}

static unsigned list_event(void *st, const dx_ui_event_t *ev)
{
    unsigned r = dx_list_event(&list, ev);

    (void)st;
    if (r & DX_UI_HANDLED) {
        log2("list sel", list.sel, "top", list.top);
    } else if (ev->type == DX_UI_UP || ev->type == DX_UI_DOWN) {
        log2("list end", list.sel, "", -1);
    }
    return r;
}

static void list_draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    char status[32] = "Row ";
    char num[12];
    int n = 4;
    const char *p;
    dx_rect_t la = below_caption(area);

    (void)st;
    caption(ctx, area, "Supplies, 12 rows");
    la.h = LIST_ROWS * DX_ROW_H;
    dx_list_draw(&list, ctx, la);
    dx_itoa(list.sel + 1, num);
    for (p = num; *p != '\0'; p++) {
        status[n++] = *p;
    }
    for (p = " of 12"; *p != '\0'; p++) {
        status[n++] = *p;
    }
    status[n] = '\0';
    ctx->gfx->text(area.x + 4, la.y + la.h + 8, status, ctx->theme.dim, DX_COLOR_NONE, 1);
}

static const char *list_hint(void *st)
{
    (void)st;
    return "UP/DOWN: select  LEFT/RIGHT: tab";
}

static const dx_screen_t s_list = {
    "LIST", list_enter, NULL, list_event, NULL, list_draw, list_hint, NULL,
};

/* ---- TEXT --------------------------------------------------------------- */

static const char sample[] =
    "Keeping a small garden is mostly a matter of patience. Seeds want warm soil, "
    "steady water and light for most of the day. Thin the seedlings early, even when "
    "it feels wasteful: crowded plants grow weak and tall.\n"
    "\n"
    "Water in the morning, so the leaves dry before night. A layer of straw keeps the "
    "soil cool and holds moisture through the hot weeks.\n"
    "\n"
    "Keep notes. Write down what you planted, where and when, and what came up. Next "
    "season those notes are worth more than any catalogue.";

static dx_text_t text;

static void text_enter(void *st)
{
    dx_rect_t body = dx_frame_body();

    (void)st;
    if (text.s == NULL) {
        dx_text_init(&text, sample, body.w, body.h - 8);
    }
}

static unsigned text_event(void *st, const dx_ui_event_t *ev)
{
    unsigned r = dx_text_event(&text, ev);

    (void)st;
    if (r & DX_UI_HANDLED) {
        log2("text top", text.top, "", -1);
    } else if (ev->type == DX_UI_UP || ev->type == DX_UI_DOWN) {
        log2("text end", text.top, "", -1);
    }
    return r;
}

static void text_draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    (void)st;
    area.y += 4;
    area.h -= 8;
    dx_text_draw(&text, ctx, area);
}

static const char *text_hint(void *st)
{
    (void)st;
    return "UP/DOWN: scroll  LEFT/RIGHT: tab";
}

static const dx_screen_t s_text = {
    "TEXT", text_enter, NULL, text_event, NULL, text_draw, text_hint, NULL,
};

/* ---- BAR ---------------------------------------------------------------- */

static void bar_draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    static const struct {
        const char *label;
        int32_t value, max;
    } bars[] = {
        { "Battery", 72, 100 },
        { "Signal", 3, 5 },
        { "Storage", 130, 120 },
        { "Unused", 4, 0 },
    };
    enum { COUNT = sizeof(bars) / sizeof(bars[0]) };
    dx_gauge_t g[COUNT];
    int cols;
    int i;

    (void)st;
    caption(ctx, area, "Gauges: value of max, clamped");
    area = below_caption(area);
    for (i = 0; i < COUNT; i++) {
        dx_gauge_init(&g[i], bars[i].label, bars[i].value, bars[i].max);
    }
    /* One value column for the group, so the bars line up. */
    cols = dx_gauge_value_cols(g, COUNT);
    for (i = 0; i < COUNT; i++) {
        dx_rect_t row = { area.x, area.y + 4 + i * 24, area.w, DX_ROW_H };

        dx_gauge_draw(&g[i], ctx, row, 8, cols);
    }
}

static const dx_screen_t s_bar = {
    "BAR", NULL, NULL, NULL, NULL, bar_draw, NULL, NULL,
};

/* ---- KV ----------------------------------------------------------------- */

static const dx_kv_row_t facts[] = {
    { "Unit", "Wrist terminal" },
    { "Serial", "DX-0451" },
    { "Firmware", "0.1.0" },
    { "Uptime", "2 h 14 min" },
    { "Owner", "Not assigned" },
    { "Location", "Unknown, no fix yet" },
};

static void kv_draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    dx_kv_t kv;

    (void)st;
    caption(ctx, area, "Keys and values");
    dx_kv_init(&kv, facts, sizeof(facts) / sizeof(facts[0]));
    dx_kv_draw(&kv, ctx, below_caption(area));
}

static const dx_screen_t s_kv = {
    "KV", NULL, NULL, NULL, NULL, kv_draw, NULL, NULL,
};

/* ---- TABS --------------------------------------------------------------- */

static const char *const sub_labels[] = { "ALL", "TOOLS", "FOOD", "MISC" };
static const char *const sub_text[] = {
    "Everything you carry.",
    "Things that fix other things.",
    "What keeps you going.",
    "The rest, sorted later.",
};

static dx_subtabs_t subs;

static void tabs_enter(void *st)
{
    (void)st;
    if (subs.count == 0) {
        dx_subtabs_init(&subs, sub_labels, 4);
    }
}

static unsigned tabs_event(void *st, const dx_ui_event_t *ev)
{
    unsigned r = dx_subtabs_event(&subs, ev);

    (void)st;
    if (r & DX_UI_HANDLED) {
        log2("sub", subs.active, "", -1);
    }
    return r;
}

static void tabs_draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    (void)st;
    dx_subtabs_draw(&subs, ctx, area);
    ctx->gfx->text(area.x + 4, area.y + DX_SUBTABS_H + 12, sub_text[subs.active], ctx->theme.fg,
                   DX_COLOR_NONE, 1);
    ctx->gfx->text(area.x + 4, area.y + DX_SUBTABS_H + 44, "LEFT at the first and RIGHT at",
                   ctx->theme.dim, DX_COLOR_NONE, 1);
    ctx->gfx->text(area.x + 4, area.y + DX_SUBTABS_H + 60, "the last change the main tab.",
                   ctx->theme.dim, DX_COLOR_NONE, 1);
}

static const dx_screen_t s_tabs = {
    "TABS", tabs_enter, NULL, tabs_event, NULL, tabs_draw, NULL, NULL,
};

/* ---- ICON --------------------------------------------------------------- */

static void icon_draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    int cell = area.w / (dx_icon_count > 0 ? dx_icon_count : 1);
    int i;

    (void)st;
    caption(ctx, area, "Icons from assets/, 24x24");
    for (i = 0; i < dx_icon_count; i++) {
        const dx_icon_t *ic = &dx_icons[i];
        dx_rect_t box = { area.x + i * cell, area.y + 50, cell, 40 };
        int nw = dx_text_width(ic->name, 1);

        ctx->gfx->rect(box.x + cell / 2 - 24, box.y - 4, 48, 48, ctx->theme.faint, 0);
        dx_icon_draw(ic, ctx, (dx_rect_t){ box.x, box.y, cell, 40 }, ctx->theme.fg);
        ctx->gfx->text(box.x + (cell - nw) / 2, box.y + 56, ic->name, ctx->theme.dim,
                       DX_COLOR_NONE, 1);
    }
}

static const dx_screen_t s_icon = {
    "ICON", NULL, NULL, NULL, NULL, icon_draw, NULL, NULL,
};

/* ---- FONT --------------------------------------------------------------- */

static void centred(const dx_ctx_t *ctx, dx_rect_t area, int y, const char *s, int unit,
                    uint32_t color)
{
    /* The last advance holds a gap after the glyph: leave it out. */
    int w = dx_vfont_width(s, unit) - 2 * unit;

    dx_vfont_draw(ctx, area.x + (area.w - w) / 2, y, s, unit, color);
}

static void font_draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    (void)st;
    centred(ctx, area, area.y + 6, "ABCDEFGHIJKLM", 4, ctx->theme.fg);
    centred(ctx, area, area.y + 36, "NOPQRSTUVWXYZ", 4, ctx->theme.fg);
    centred(ctx, area, area.y + 66, "0123456789", 4, ctx->theme.fg);
    centred(ctx, area, area.y + 96, ": - . / % #", 4, ctx->theme.dim);
    centred(ctx, area, area.y + 132, "12:45", 8, ctx->theme.fg);
}

static const dx_screen_t s_font = {
    "FONT", NULL, NULL, NULL, NULL, font_draw, NULL, NULL,
};

/* ---- The table ---------------------------------------------------------- */

const dx_screen_t *const dx_rom_screens[] = {
    &s_list, &s_text, &s_bar, &s_kv, &s_tabs, &s_icon, &s_font,
};

const int dx_rom_screen_count = sizeof(dx_rom_screens) / sizeof(dx_rom_screens[0]);
