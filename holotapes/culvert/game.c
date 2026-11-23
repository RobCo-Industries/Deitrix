/*
 * Culvert's state machine. See game.h.
 */
#include "game.h"

static void set_title(cv_game_t *g)
{
    g->st = CV_ST_TITLE;
    g->menu = 0;
    g->fx |= CV_FX_DRAW_ALL;
    g->log = CV_LOG_TITLE;
}

void cv_game_init(cv_game_t *g, const uint8_t *save, uint32_t len)
{
    int i;

    g->st = CV_ST_TITLE;
    g->paused_from = CV_ST_PLAY;
    g->lv.w = 0;
    g->lv.h = 0;
    for (i = 0; i < CV_MAX_TILES; i++) {
        g->conn[i] = 0;
    }
    g->nconn = 0;
    g->cx = g->cy = g->px = g->py = g->tx = g->ty = 0;
    g->moves = 0;
    g->start = g->frozen = g->shown_s = g->solved_at = 0;
    g->frame = 0;
    g->fx = 0;
    g->log = CV_LOG_NONE;
    (void)cv_save_decode(&g->save, save, len);
    set_title(g);
}

int cv_menu_count(const cv_game_t *g)
{
    if (g->st == CV_ST_PAUSE) {
        return 3;
    }
    return g->save.highest > 1 ? 3 : 2;
}

cv_item_t cv_menu_item(const cv_game_t *g, int i)
{
    static const cv_item_t pause_items[3] = { CV_ITEM_RESUME, CV_ITEM_RESTART, CV_ITEM_QUIT };
    static const cv_item_t title_items[3] = { CV_ITEM_CONTINUE, CV_ITEM_NEW, CV_ITEM_QUIT };

    if (i < 0) {
        i = 0;
    }
    if (g->st == CV_ST_PAUSE) {
        return pause_items[i > 2 ? 2 : i];
    }
    if (g->save.highest <= 1) {
        i++;
    }
    return title_items[i > 2 ? 2 : i];
}

uint32_t cv_game_time(const cv_game_t *g, uint32_t now)
{
    if (g->st == CV_ST_PLAY) {
        return cv_elapsed(g->start, now);
    }
    return g->frozen;
}

static void start_level(cv_game_t *g, uint32_t level, uint32_t now)
{
    if (level == 0) {
        level = 1;
    }
    if (level > CV_LEVEL_MAX) {
        level = CV_LEVEL_MAX;
    }
    cv_generate(&g->lv, level);
    g->nconn = cv_flood(&g->lv, g->conn);
    g->st = CV_ST_PLAY;
    g->cx = g->px = g->lv.sx;
    g->cy = g->py = g->lv.sy;
    g->moves = 0;
    g->start = now & 0x7FFFFFFFu;
    g->frozen = 0;
    g->shown_s = 0;
    g->frame = 0;
    g->fx |= CV_FX_DRAW_ALL;
    g->log = CV_LOG_LEVEL;
}

static void quit(cv_game_t *g)
{
    g->st = CV_ST_EXIT;
    g->fx |= CV_FX_EXIT;
    g->log = CV_LOG_EXIT;
}

static void solved(cv_game_t *g, uint32_t now)
{
    int cls = cv_class(g->lv.level);
    uint32_t next = g->lv.level + 1;

    g->frozen = cv_elapsed(g->start, now);
    g->st = CV_ST_SOLVED;
    g->solved_at = now & 0x7FFFFFFFu;
    g->frame = 0;
    if (next > CV_LEVEL_MAX) {
        next = CV_LEVEL_MAX;
    }
    if (next > g->save.highest) {
        g->save.highest = (uint16_t)next;
    }
    if (g->moves < g->save.best[cls]) {
        g->save.best[cls] = (uint16_t)(g->moves > 0xFFFEu ? 0xFFFEu : g->moves);
    }
    g->fx |= CV_FX_DRAW_GRID | CV_FX_TUNE | CV_FX_SAVE;
    g->log = CV_LOG_SOLVED;
}

/* Turns the tile under the cursor one quarter at a time, in the sign's
 * direction. Each quarter counts as a move. The first quarter that solves
 * the level ends the turn; the rest are dropped. */
static void turn(cv_game_t *g, int quarters, uint32_t now)
{
    int before = g->nconn;
    int dir = quarters < 0 ? -1 : 1;
    int n = quarters < 0 ? -quarters : quarters;

    g->tx = g->cx;
    g->ty = g->cy;
    while (n-- > 0) {
        cv_turn(&g->lv, g->cx, g->cy, dir);
        g->moves++;
        g->nconn = cv_flood(&g->lv, g->conn);
        if (cv_solved(&g->lv)) {
            solved(g, now);
            return;
        }
    }
    g->fx |= CV_FX_DRAW_GRID | (g->nconn > before ? CV_FX_CONNECT : CV_FX_TICK);
    g->log = CV_LOG_TURN;
}

static void move_cursor(cv_game_t *g, int dx, int dy)
{
    int x = g->cx + dx;
    int y = g->cy + dy;

    if (x < 0 || y < 0 || x >= g->lv.w || y >= g->lv.h) {
        g->log = CV_LOG_IDLE;
        return;
    }
    g->px = g->cx;
    g->py = g->cy;
    g->cx = x;
    g->cy = y;
    g->fx |= CV_FX_DRAW_CURSOR;
    g->log = CV_LOG_CURSOR;
}

static int move_menu(cv_game_t *g, int d)
{
    int m = g->menu + d;

    if (m < 0 || m >= cv_menu_count(g)) {
        g->log = CV_LOG_IDLE;
        return 0;
    }
    g->menu = m;
    g->fx |= CV_FX_DRAW_ALL;
    g->log = CV_LOG_MENU;
    return 1;
}

static void pause(cv_game_t *g, uint32_t now)
{
    if (g->st == CV_ST_PLAY) {
        g->frozen = cv_elapsed(g->start, now);
    }
    g->paused_from = g->st;
    g->st = CV_ST_PAUSE;
    g->menu = 0;
    g->fx |= CV_FX_DRAW_ALL;
    g->log = CV_LOG_PAUSE;
}

static void resume(cv_game_t *g, uint32_t now)
{
    g->st = g->paused_from;
    if (g->st == CV_ST_PLAY) {
        g->start = (now - g->frozen) & 0x7FFFFFFFu;
    } else {
        g->frame = CV_FLOURISH_FRAMES;
    }
    g->fx |= CV_FX_DRAW_ALL;
    g->log = CV_LOG_RESUME;
}

static void title_key(cv_game_t *g, uint32_t code, uint32_t now)
{
    switch (code) {
    case CV_KEY_UP:
        move_menu(g, -1);
        break;
    case CV_KEY_DOWN:
        move_menu(g, 1);
        break;
    case CV_KEY_SELECT:
        switch (cv_menu_item(g, g->menu)) {
        case CV_ITEM_CONTINUE:
            start_level(g, g->save.highest, now);
            break;
        case CV_ITEM_NEW:
            start_level(g, 1, now);
            break;
        default:
            quit(g);
            break;
        }
        break;
    case CV_KEY_BACK:
        quit(g);
        break;
    default:
        g->log = CV_LOG_IDLE;
        break;
    }
}

static void pause_key(cv_game_t *g, uint32_t code, uint32_t now)
{
    switch (code) {
    case CV_KEY_UP:
        move_menu(g, -1);
        break;
    case CV_KEY_DOWN:
        move_menu(g, 1);
        break;
    case CV_KEY_BACK:
        resume(g, now);
        break;
    case CV_KEY_SELECT:
        switch (cv_menu_item(g, g->menu)) {
        case CV_ITEM_RESUME:
            resume(g, now);
            break;
        case CV_ITEM_RESTART:
            start_level(g, g->lv.level, now);
            break;
        default:
            quit(g);
            break;
        }
        break;
    default:
        g->log = CV_LOG_IDLE;
        break;
    }
}

static void play_key(cv_game_t *g, uint32_t code, int32_t value, uint32_t now)
{
    switch (code) {
    case CV_KEY_UP:
        move_cursor(g, 0, -1);
        break;
    case CV_KEY_DOWN:
        move_cursor(g, 0, 1);
        break;
    case CV_KEY_LEFT:
        move_cursor(g, -1, 0);
        break;
    case CV_KEY_RIGHT:
        move_cursor(g, 1, 0);
        break;
    case CV_KEY_SELECT:
        if (value == CV_PRESS) {
            turn(g, 1, now);
        }
        break;
    case CV_KEY_BACK:
        if (value == CV_PRESS) {
            pause(g, now);
        }
        break;
    default:
        if (value == CV_PRESS) {
            g->log = CV_LOG_IDLE;
        }
        break;
    }
}

static void solved_key(cv_game_t *g, uint32_t code, uint32_t now)
{
    if (code == CV_KEY_SELECT && g->frame >= CV_FLOURISH_FRAMES) {
        start_level(g, g->lv.level + 1, now);
    } else if (code == CV_KEY_BACK) {
        pause(g, now);
    } else {
        g->log = CV_LOG_IDLE;
    }
}

void cv_game_event(cv_game_t *g, const cv_input_t *ev, uint32_t now)
{
    uint32_t code;

    g->fx = 0;
    g->log = CV_LOG_NONE;
    g->px = g->cx;
    g->py = g->cy;
    if (ev->type == CV_EV_DIAL) {
        if (g->st == CV_ST_PLAY && ev->value != 0) {
            /* One quarter per detent. Four quarters bring the tile back,
             * so only the count modulo 4 is applied, in the dial's
             * direction. C keeps the sign of the dividend. */
            int q = (int)(ev->value % 4);

            if (q == 0) {
                g->log = CV_LOG_IDLE;
            } else {
                turn(g, q, now);
            }
        } else if (g->st == CV_ST_TITLE || g->st == CV_ST_PAUSE) {
            move_menu(g, ev->value > 0 ? 1 : -1);
        } else {
            g->log = CV_LOG_IDLE;
        }
        return;
    }
    if (ev->type != CV_EV_BUTTON || ev->value == CV_RELEASE) {
        return;
    }
    /* Only the arrows repeat. */
    if (ev->value == CV_REPEAT && ev->code > CV_KEY_RIGHT) {
        return;
    }
    /* The dial push acts as SELECT everywhere. */
    code = ev->code == CV_KEY_DIAL ? CV_KEY_SELECT : ev->code;
    switch (g->st) {
    case CV_ST_TITLE:
        title_key(g, code, now);
        break;
    case CV_ST_PLAY:
        play_key(g, code, ev->value, now);
        break;
    case CV_ST_PAUSE:
        pause_key(g, code, now);
        break;
    case CV_ST_SOLVED:
        solved_key(g, code, now);
        break;
    default:
        break;
    }
}

void cv_game_tick(cv_game_t *g, uint32_t now)
{
    g->fx = 0;
    g->log = CV_LOG_NONE;
    g->px = g->cx;
    g->py = g->cy;
    if (g->st == CV_ST_PLAY) {
        uint32_t s = cv_elapsed(g->start, now) / 1000u;

        if (s != g->shown_s) {
            g->shown_s = s;
            g->fx |= CV_FX_DRAW_STATUS;
        }
    } else if (g->st == CV_ST_SOLVED && g->frame < CV_FLOURISH_FRAMES) {
        uint32_t f = cv_elapsed(g->solved_at, now) / CV_FLOURISH_MS;

        if (f >= CV_FLOURISH_FRAMES) {
            g->frame = CV_FLOURISH_FRAMES;
            g->fx |= CV_FX_DRAW_ALL;
            g->log = CV_LOG_DONE;
        } else if ((int)f != g->frame) {
            g->frame = (int)f;
            g->fx |= CV_FX_DRAW_GRID;
        }
    }
}

uint32_t cv_game_wait(const cv_game_t *g, uint32_t now)
{
    if (g->st == CV_ST_PLAY) {
        return 1000u - cv_elapsed(g->start, now) % 1000u;
    }
    if (g->st == CV_ST_SOLVED && g->frame < CV_FLOURISH_FRAMES) {
        uint32_t e = cv_elapsed(g->solved_at, now);
        uint32_t next = ((uint32_t)g->frame + 1u) * CV_FLOURISH_MS;

        return e >= next ? 1u : next - e;
    }
    return CV_WAIT_FOREVER;
}
