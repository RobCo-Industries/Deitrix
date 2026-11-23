/*
 * Culvert's screens. The logical surface is 320x240: a title row, the
 * grid and a status row. Connected pipes draw in full brightness, the
 * others dim. See docs/CULVERT.md.
 */
#include <pipos/pip.h>

#include "draw.h"

#define SCREEN_W 320
#define GRID_X   12
#define GRID_Y   32
#define GRID_W   296
#define GRID_H   176
#define TILE_MAX 40
#define STATUS_Y 222
#define CHAR_W   8

static uint32_t fg, dim, faint;

void cv_draw_init(void)
{
    pip_gfx_info_t gi;

    fg = pip_gfx_info(&gi) == 0 ? gi.fg_default : 0x33E066u;
    dim = (fg >> 1) & 0x7F7F7Fu;
    faint = (fg >> 2) & 0x3F3F3Fu;
}

static int text_w(const char *s, int scale)
{
    return (int)strlen(s) * CHAR_W * scale;
}

static void text(int x, int y, const char *s, uint32_t color, int scale)
{
    pip_set_color(color, PIP_COLOR_NONE);
    pip_draw_text(x, y, s, (uint32_t)scale);
}

static void text_center(int y, const char *s, uint32_t color, int scale)
{
    text((SCREEN_W - text_w(s, scale)) / 2, y, s, color, scale);
}

static void fill(int x, int y, int w, int h, uint32_t color)
{
    pip_set_color(color, PIP_COLOR_NONE);
    pip_rect_xywh(x, y, (uint32_t)w, (uint32_t)h, PIP_RECT_FILLED);
}

static void frame(int x, int y, int w, int h, uint32_t color)
{
    pip_set_color(color, PIP_COLOR_NONE);
    pip_rect_xywh(x, y, (uint32_t)w, (uint32_t)h, PIP_RECT_OUTLINE);
}

/* The tile size and the grid's top left corner for this level. */
static int tile_size(const cv_level_t *lv)
{
    int a = GRID_W / lv->w;
    int b = GRID_H / lv->h;
    int t = (a < b ? a : b) & ~3;

    return t > TILE_MAX ? TILE_MAX : t;
}

static void grid_origin(const cv_level_t *lv, int t, int *x0, int *y0)
{
    *x0 = GRID_X + (GRID_W - t * lv->w) / 2;
    *y0 = GRID_Y + (GRID_H - t * lv->h) / 2;
}

/* One pipe piece: a square hub with an arm to each open side. A cap gets
 * a wider hub, the source a ring. */
static void draw_piece(int x, int y, int t, uint8_t mask, uint32_t color, int source)
{
    int a = (t / 4) & ~1;
    int c = (t - a) / 2;

    fill(x, y, t, t, 0);
    frame(x, y, t, t, faint);
    if (mask & CV_N) {
        fill(x + c, y, a, c + a, color);
    }
    if (mask & CV_S) {
        fill(x + c, y + c, a, t - c, color);
    }
    if (mask & CV_W) {
        fill(x, y + c, c + a, a, color);
    }
    if (mask & CV_E) {
        fill(x + c, y + c, t - c, a, color);
    }
    if (source) {
        int r = a + t / 4;
        int h = (t - r) / 2;

        fill(x + h, y + h, r, r, color);
        fill(x + h + 2, y + h + 2, r - 4, r - 4, 0);
        fill(x + c + 1, y + c + 1, a - 2, a - 2, color);
    } else if (cv_piece(mask) == CV_PIECE_CAP) {
        int r = a + t / 6;
        int h = (t - r) / 2;

        fill(x + h, y + h, r, r, color);
    } else {
        fill(x + c, y + c, a, a, color);
    }
}

static void draw_tile(const cv_game_t *g, int tx, int ty)
{
    const cv_level_t *lv = &g->lv;
    int t = tile_size(lv);
    int x0, y0, x, y, i = ty * lv->w + tx;
    uint32_t color = g->conn[i] ? fg : dim;

    grid_origin(lv, t, &x0, &y0);
    x = x0 + tx * t;
    y = y0 + ty * t;
    if (g->st == CV_ST_SOLVED && g->frame < CV_FLOURISH_FRAMES) {
        color = (g->frame & 1) ? dim : fg;
    }
    draw_piece(x, y, t, lv->mask[i], color, tx == lv->sx && ty == lv->sy);
    if (g->st == CV_ST_PLAY && tx == g->cx && ty == g->cy) {
        frame(x + 1, y + 1, t - 2, t - 2, fg);
        frame(x + 2, y + 2, t - 4, t - 4, fg);
    }
}

void cv_draw_grid(const cv_game_t *g)
{
    int x, y;

    for (y = 0; y < g->lv.h; y++) {
        for (x = 0; x < g->lv.w; x++) {
            draw_tile(g, x, y);
        }
    }
}

static void draw_header(void)
{
    fill(0, 0, SCREEN_W, 28, 0);
    text(GRID_X, 6, "CULVERT", fg, 2);
    text(SCREEN_W - GRID_X - text_w("BACK: menu", 1), 10, "BACK: menu", dim, 1);
    fill(0, 27, SCREEN_W, 1, faint);
}

static void draw_status(const cv_game_t *g, uint32_t now)
{
    char line[24];
    char num[11];
    char tm[9];

    fill(0, 212, SCREEN_W, 28, 0);
    fill(0, 212, SCREEN_W, 1, faint);
    pip_snprintf(line, sizeof line, "Level %u", (unsigned)g->lv.level);
    text(GRID_X, STATUS_Y, line, fg, 1);
    cv_fmt_u32(num, g->moves);
    pip_snprintf(line, sizeof line, "Moves %s", num);
    text(128, STATUS_Y, line, fg, 1);
    cv_fmt_time(tm, cv_game_time(g, now));
    text(SCREEN_W - GRID_X - text_w(tm, 1), STATUS_Y, tm, fg, 1);
}

/* A menu row: the selected one is a bar with dark text. */
static void draw_item(int y, int w, const char *s, int selected)
{
    int x = (SCREEN_W - w) / 2;

    if (selected) {
        fill(x, y - 5, w, 18, fg);
        text_center(y, s, 0, 1);
    } else {
        text_center(y, s, fg, 1);
    }
}

static const char *item_text(const cv_game_t *g, cv_item_t it, char *buf, int size)
{
    switch (it) {
    case CV_ITEM_CONTINUE:
        pip_snprintf(buf, (size_t)size, "Continue (level %u)", (unsigned)g->save.highest);
        return buf;
    case CV_ITEM_NEW:
        return "New game";
    case CV_ITEM_RESUME:
        return "Resume";
    case CV_ITEM_RESTART:
        return "Restart level";
    default:
        return "Quit";
    }
}

static void draw_title(const cv_game_t *g)
{
    static const uint8_t strip[7] = {
        CV_E, CV_E | CV_W, CV_E | CV_W | CV_S, CV_E | CV_W, CV_W | CV_S, CV_E, CV_W,
    };
    char buf[28];
    int i;

    pip_clear(0);
    text_center(30, "CULVERT", fg, 4);
    for (i = 0; i < 7; i++) {
        draw_piece(76 + i * 24, 76, 24, strip[i], i < 4 ? fg : dim, i == 0);
    }
    text_center(114, "Turn the pipes. Fill every tile.", dim, 1);
    for (i = 0; i < cv_menu_count(g); i++) {
        draw_item(146 + i * 22, 192, item_text(g, cv_menu_item(g, i), buf, sizeof buf),
                  i == g->menu);
    }
    text_center(222, "UP/DOWN: choose  SELECT: confirm", dim, 1);
}

static void draw_box(int w, int h)
{
    int x = (SCREEN_W - w) / 2;
    int y = GRID_Y + (GRID_H - h) / 2;

    fill(x, y, w, h, 0);
    frame(x, y, w, h, fg);
    frame(x + 2, y + 2, w - 4, h - 4, dim);
}

static void draw_pause(const cv_game_t *g)
{
    char buf[28];
    int i;
    int y = GRID_Y + (GRID_H - 108) / 2;

    draw_box(184, 108);
    text_center(y + 12, "PAUSED", fg, 2);
    for (i = 0; i < cv_menu_count(g); i++) {
        draw_item(y + 44 + i * 22, 152, item_text(g, cv_menu_item(g, i), buf, sizeof buf),
                  i == g->menu);
    }
}

static void draw_done(const cv_game_t *g)
{
    char line[40];
    char num[11];
    int y = GRID_Y + (GRID_H - 60) / 2;

    draw_box(288, 60);
    cv_fmt_u32(num, g->moves);
    pip_snprintf(line, sizeof line, "Level %u done in %s move%s", (unsigned)g->lv.level, num,
                 g->moves == 1 ? "" : "s");
    text_center(y + 16, line, fg, 1);
    text_center(y + 36, "SELECT: next level", dim, 1);
}

static void draw_play(const cv_game_t *g, uint32_t now)
{
    pip_clear(0);
    draw_header();
    cv_draw_grid(g);
    draw_status(g, now);
    if (g->st == CV_ST_PAUSE) {
        draw_pause(g);
    } else if (g->st == CV_ST_SOLVED && g->frame >= CV_FLOURISH_FRAMES) {
        draw_done(g);
    }
}

void cv_draw(const cv_game_t *g, uint32_t now)
{
    uint32_t fx = g->fx;

    if (!(fx & (CV_FX_DRAW_ALL | CV_FX_DRAW_GRID | CV_FX_DRAW_CURSOR | CV_FX_DRAW_STATUS))) {
        return;
    }
    if (g->st == CV_ST_EXIT) {
        return;
    }
    if (fx & CV_FX_DRAW_ALL) {
        if (g->st == CV_ST_TITLE) {
            draw_title(g);
        } else {
            draw_play(g, now);
        }
    } else {
        if (fx & CV_FX_DRAW_GRID) {
            cv_draw_grid(g);
            fx |= CV_FX_DRAW_STATUS;
        } else if (fx & CV_FX_DRAW_CURSOR) {
            draw_tile(g, g->px, g->py);
            draw_tile(g, g->cx, g->cy);
        }
        if (fx & CV_FX_DRAW_STATUS) {
            draw_status(g, now);
        }
    }
    pip_present();
}
