/*
 * The MAP tab: one sub-tab per map in MAP.INI, LOCAL then WORLD. The
 * map view draws the lines and the places fitted to the body, with the
 * selected place's row under them. SELECT opens the place's detail.
 * Nothing ticks: the screen redraws only after a key.
 */
#include "screens/screens.h"

#include "content.h"
#include "model/map_view.h"
#include "model/util.h"
#include "screens/map_screen.h"
#include "ui/kv.h"
#include "ui/subtabs.h"

#define PAD      4
#define LOG_SIZE 96

typedef struct map_state {
    int ready;
    int count; /* maps shown: 0 when the file did not load */
    const char *labels[DX_MAPS_MAX];
    dx_subtabs_t subs;
    int sel[DX_MAPS_MAX]; /* each map's selected place */
    int detail;           /* 1 while the selected place's detail is open */
    int silent;           /* the first frame after the tab opens logs nothing */
    char logged[LOG_SIZE];
    char hint[48];
} map_state_t;

static map_state_t map;

static const dx_maps_t *maps(void)
{
    return dx_content_maps();
}

static const char *label(const dx_map_t *m)
{
    return m->which == DX_MAP_WORLD ? "WORLD" : "LOCAL";
}

/* The shell enters the first tab before the content loads, so the map
 * count is read again each time. */
static void setup(void)
{
    const dx_maps_t *m = maps();
    int count = m->load.status == DX_LOAD_OK ? m->count : 0;
    int i;

    if (map.ready && map.count == count) {
        return;
    }
    map.ready = 1;
    map.count = count;
    map.detail = 0;
    for (i = 0; i < count; i++) {
        map.labels[i] = label(&m->maps[i]);
        map.sel[i] = m->maps[i].place_count > 0 ? 0 : -1;
    }
    dx_subtabs_init(&map.subs, map.labels, count);
}

void dx_map_reset(void)
{
    dx_mem_set(&map, 0, sizeof(map));
}

static const dx_map_t *shown(void)
{
    return map.count > 0 ? &maps()->maps[map.subs.active] : NULL;
}

/* Logs a view's line when it differs from the last. The first frame
 * after the tab opens only records it, so the shell's own lines stay
 * the only ones then, as on STAT. */
static void log_once(const char *line)
{
    if (dx_str_eq(line, map.logged)) {
        return;
    }
    dx_str_cut(map.logged, sizeof(map.logged), line, dx_str_len(line));
    if (!map.silent) {
        dx_plat_log(line);
    }
}

static void enter(void *st)
{
    (void)st;
    setup();
    map.logged[0] = '\0';
    map.silent = 1;
}

/* Leaving closes the detail; the sub-tab and the selections stay. */
static void leave(void *st)
{
    (void)st;
    map.detail = 0;
    map.logged[0] = '\0';
}

static unsigned event(void *st, const dx_ui_event_t *ev)
{
    const dx_map_t *m;
    int *sel;
    int next;
    unsigned r;

    (void)st;
    setup();
    if (map.detail) {
        if (dx_ui_is_back(ev)) {
            map.detail = 0;
            return DX_UI_HANDLED | DX_UI_DIRTY;
        }
        /* RIGHT stays in the detail: no tab change. */
        return ev->type == DX_UI_RIGHT ? DX_UI_HANDLED : DX_UI_IGNORED;
    }
    r = dx_subtabs_event(&map.subs, ev);
    if (r != DX_UI_IGNORED) {
        return r;
    }
    m = shown();
    if (m == NULL) {
        return DX_UI_IGNORED;
    }
    sel = &map.sel[map.subs.active];
    switch (ev->type) {
    case DX_UI_UP:
    case DX_UI_DOWN:
        next = dx_map_cycle(*sel, m->place_count, ev->type == DX_UI_UP ? -1 : 1);
        if (next == *sel) {
            return DX_UI_IGNORED; /* no place, or only one */
        }
        *sel = next;
        return DX_UI_HANDLED | DX_UI_DIRTY;
    case DX_UI_SELECT:
        if (*sel < 0) {
            return DX_UI_IGNORED;
        }
        map.detail = 1;
        return DX_UI_HANDLED | DX_UI_DIRTY;
    default:
        return DX_UI_IGNORED;
    }
}

/* A centred message in the area, and a dim second line when given. */
static void message(const dx_ctx_t *ctx, dx_rect_t r, const char *a, const char *b)
{
    int y = r.y + (r.h - DX_ROW_H) / 2 - (b != NULL ? DX_ROW_H / 2 : 0);

    ctx->gfx->text(r.x + (r.w - dx_text_width(a, 1)) / 2, y, a, ctx->theme.fg, DX_COLOR_NONE,
                   1);
    if (b != NULL) {
        ctx->gfx->text(r.x + (r.w - dx_text_width(b, 1)) / 2, y + DX_ROW_H, b, ctx->theme.dim,
                       DX_COLOR_NONE, 1);
    }
}

static void draw_empty(const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_maps_t *m = maps();
    char why[40];
    const char *reason;

    log_once("deitrix: map shows none\n");
    if (m->load.status == DX_LOAD_OK) {
        reason = "MAP.INI holds no map.";
    } else {
        reason = dx_file_reason(&m->load, "MAP.INI", why, sizeof(why));
    }
    message(ctx, area, "No map on the card.", reason);
}

static void fill_square(const dx_ctx_t *ctx, dx_map_square_t s, uint32_t color)
{
    ctx->gfx->rect(s.x, s.y, s.size, s.size, color, 1);
}

/*
 * Draws map m fitted into the pane, clipped to it: the lines, the
 * places in `place` colour, then the selected place, if any, as a
 * reticle.
 */
static void draw_map(const dx_ctx_t *ctx, const dx_map_t *m, dx_rect_t pane, int sel,
                     uint32_t place)
{
    dx_map_fit_t f = dx_map_pane_fit(m, pane.x, pane.y, pane.w, pane.h);
    int16_t xy[2 * DX_POINTS_MAX];
    int i, px, py;

    ctx->gfx->clip(pane.x, pane.y, pane.w, pane.h);
    for (i = 0; i < m->line_count; i++) {
        int n = dx_map_line_px(&f, &m->lines[i], xy);

        ctx->gfx->polyline(xy, n, 0, ctx->theme.dim);
    }
    for (i = 0; i < m->place_count; i++) {
        if (i == sel) {
            continue;
        }
        dx_map_to_px(&f, m->places[i].at.x, m->places[i].at.y, &px, &py);
        fill_square(ctx, dx_map_square(px, py, DX_MAP_MARK), place);
    }
    if (sel >= 0 && sel < m->place_count) {
        dx_map_square_t s;

        dx_map_to_px(&f, m->places[sel].at.x, m->places[sel].at.y, &px, &py);
        s = dx_map_square(px, py, DX_MAP_MARK_SEL);
        /* A bg square first, so lines under the ring do not cross it. */
        fill_square(ctx, s, ctx->theme.bg);
        ctx->gfx->rect(s.x, s.y, s.size, s.size, ctx->theme.fg, 0);
        fill_square(ctx, dx_map_square(px, py, DX_MAP_DOT), ctx->theme.fg);
        ctx->gfx->line(px, py - DX_MAP_REACH, px, s.y - 1, ctx->theme.fg);
        ctx->gfx->line(px, s.y + s.size, px, py + DX_MAP_REACH, ctx->theme.fg);
        ctx->gfx->line(px - DX_MAP_REACH, py, s.x - 1, py, ctx->theme.fg);
        ctx->gfx->line(s.x + s.size, py, px + DX_MAP_REACH, py, ctx->theme.fg);
    }
    ctx->gfx->clip(0, 0, 0, 0);
}

static void draw_view(const dx_ctx_t *ctx, dx_rect_t area, dx_rect_t r)
{
    const dx_map_t *m = shown();
    int sel = map.sel[map.subs.active];
    dx_rect_t pane = { r.x, r.y, r.w, r.h - DX_MAP_INFO_H };
    int ty = r.y + r.h - DX_ROW_H;
    char line[LOG_SIZE];
    dx_sb_t sb;
    uint32_t t0 = dx_plat_ticks();

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: map shows ");
    dx_sb_str(&sb, label(m));
    dx_sb_str(&sb, " place=");
    dx_sb_int(&sb, sel);
    dx_sb_char(&sb, '\n');
    log_once(line);

    dx_subtabs_draw(&map.subs, ctx, area);
    ctx->gfx->text(area.x + area.w - PAD - dx_text_width(m->title, 1), area.y + 1, m->title,
                   ctx->theme.dim, DX_COLOR_NONE, 1);
    draw_map(ctx, m, pane, sel, ctx->theme.fg);
    ctx->gfx->line(r.x + PAD, pane.y + pane.h, r.x + r.w - 1 - PAD, pane.y + pane.h,
                   ctx->theme.faint);
    if (sel < 0) {
        const char *none = "No places on this map.";

        ctx->gfx->text(r.x + (r.w - dx_text_width(none, 1)) / 2, ty, none, ctx->theme.dim,
                       DX_COLOR_NONE, 1);
    } else {
        const char *kind = dx_place_kind_name(m->places[sel].kind);

        ctx->gfx->text(r.x + PAD, ty, m->places[sel].name, ctx->theme.fg, DX_COLOR_NONE, 1);
        ctx->gfx->text(r.x + r.w - PAD - dx_text_width(kind, 1), ty, kind, ctx->theme.dim,
                       DX_COLOR_NONE, 1);
    }

    /* The time the map view took, for the redraw cost on the boards. */
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: map drew ");
    dx_sb_uint(&sb, dx_ticks_since(t0, dx_plat_ticks()));
    dx_sb_str(&sb, " ms\n");
    if (!map.silent) {
        dx_plat_log(line);
    }
}

static void draw_detail(const dx_ctx_t *ctx, dx_rect_t area, dx_rect_t r)
{
    const dx_map_t *m = shown();
    int sel = map.sel[map.subs.active];
    const dx_place_t *p = &m->places[sel];
    char pos[DX_MAP_POS_SIZE];
    char nth[24];
    char line[LOG_SIZE];
    dx_kv_row_t rows[DX_MAP_KV_ROWS];
    dx_kv_t kv;
    dx_sb_t sb;
    dx_rect_t loc;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: map detail ");
    dx_sb_str(&sb, label(m));
    dx_sb_str(&sb, " place=");
    dx_sb_int(&sb, sel);
    dx_sb_char(&sb, '\n');
    log_once(line);

    /* The title row in place of the sub-tabs. */
    ctx->gfx->text(area.x + PAD, area.y + 1, p->name, ctx->theme.fg, DX_COLOR_NONE, 1);
    ctx->gfx->line(area.x, r.y - 1, area.x + area.w - 1, r.y - 1, ctx->theme.faint);

    dx_sb_init(&sb, nth, sizeof(nth));
    dx_sb_int(&sb, sel + 1);
    dx_sb_str(&sb, " of ");
    dx_sb_int(&sb, m->place_count);
    rows[0] = (dx_kv_row_t){ "Kind", dx_place_kind_name(p->kind) };
    rows[1] = (dx_kv_row_t){ "Position", dx_map_pos_text(p->at, pos) };
    rows[2] = (dx_kv_row_t){ "Map", m->title };
    rows[3] = (dx_kv_row_t){ "Place", nth };
    dx_kv_init(&kv, rows, DX_MAP_KV_ROWS);
    dx_kv_draw(&kv, ctx, (dx_rect_t){ r.x, r.y + DX_MAP_KV_DY, r.w, DX_MAP_KV_ROWS * DX_ROW_H });

    /* A small locator: the whole map, the other places dim. */
    loc.w = DX_MAP_LOC_W;
    loc.x = r.x + (r.w - loc.w) / 2;
    loc.y = r.y + DX_MAP_LOC_DY;
    loc.h = r.y + r.h - 2 - loc.y;
    ctx->gfx->rect(loc.x, loc.y, loc.w, loc.h, ctx->theme.faint, 0);
    draw_map(ctx, m, (dx_rect_t){ loc.x + 1, loc.y + 1, loc.w - 2, loc.h - 2 }, sel,
             ctx->theme.dim);
}

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    dx_rect_t r = { area.x, area.y + DX_SUBTABS_H, area.w, area.h - DX_SUBTABS_H };

    (void)st;
    setup();
    if (map.count == 0) {
        draw_empty(ctx, area);
    } else if (map.detail) {
        draw_detail(ctx, area, r);
    } else {
        draw_view(ctx, area, r);
    }
    map.silent = 0;
}

static void hint_part(dx_sb_t *sb, const char *s)
{
    if (sb->len > 0) {
        dx_sb_str(sb, "  ");
    }
    dx_sb_str(sb, s);
}

/* What the keys do now, at most 40 characters. */
static const char *hint(void *st)
{
    const dx_map_t *m;
    int places;
    dx_sb_t sb;

    (void)st;
    setup();
    m = shown();
    if (m == NULL) {
        return NULL;
    }
    if (map.detail) {
        return "BACK/LEFT: back to the map";
    }
    places = m->place_count;
    if (places == 0 && map.count == 1) {
        return NULL; /* the shell's own hint */
    }
    dx_sb_init(&sb, map.hint, sizeof(map.hint));
    if (places > 1) {
        hint_part(&sb, "UP/DOWN: place");
    }
    if (places > 0) {
        hint_part(&sb, "SELECT: open");
    }
    if (map.count > 1) {
        hint_part(&sb, map.subs.active == 0 ? "RIGHT: map" : "LEFT: map");
    }
    if (places == 0) {
        hint_part(&sb, "DIAL: tab");
    }
    return map.hint;
}

const dx_screen_t dx_screen_map = {
    "MAP", enter, leave, event, NULL, draw, hint, NULL,
};
