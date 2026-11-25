/* Culvert's state machine, driven by events and ticks. */
#include "rc_test.h"

#include "game.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static cv_game_t g;

static void ev(uint32_t type, uint32_t code, int32_t value, uint32_t now)
{
    cv_input_t in = { type, code, value };

    cv_game_event(&g, &in, now);
}

static void key(uint32_t code, uint32_t now)
{
    ev(CV_EV_BUTTON, code, CV_PRESS, now);
}

static void start_fresh(uint32_t now)
{
    cv_game_init(&g, 0, 0);
    key(CV_KEY_SELECT, now);
}

/* Walks the cursor to (x, y) with the arrows. */
static void go(int x, int y, uint32_t now)
{
    while (g.cx < x) {
        key(CV_KEY_RIGHT, now);
    }
    while (g.cx > x) {
        key(CV_KEY_LEFT, now);
    }
    while (g.cy < y) {
        key(CV_KEY_DOWN, now);
    }
    while (g.cy > y) {
        key(CV_KEY_UP, now);
    }
}

/* Turns every tile to the generator's solution with SELECT. Returns the
 * number of turns. */
static uint32_t solve(uint32_t now)
{
    uint32_t turns = 0;
    int x, y;

    for (y = 0; y < g.lv.h; y++) {
        for (x = 0; x < g.lv.w; x++) {
            int n = cv_turns_to(g.lv.mask[y * g.lv.w + x], g.lv.goal[y * g.lv.w + x]);

            go(x, y, now);
            while (n-- > 0 && g.st == CV_ST_PLAY) {
                key(CV_KEY_SELECT, now);
                turns++;
            }
        }
    }
    return turns;
}

TEST(culvert_title_fresh)
{
    cv_game_init(&g, 0, 0);
    CHECK_INT(g.st, CV_ST_TITLE);
    CHECK_INT(g.log, CV_LOG_TITLE);
    CHECK(g.fx & CV_FX_DRAW_ALL);
    CHECK_INT(cv_menu_count(&g), 2);
    CHECK_INT(cv_menu_item(&g, 0), CV_ITEM_NEW);
    CHECK_INT(cv_menu_item(&g, 1), CV_ITEM_QUIT);
    CHECK_INT(cv_game_wait(&g, 5), CV_WAIT_FOREVER);
    key(CV_KEY_UP, 0);
    CHECK_INT(g.log, CV_LOG_IDLE);
    key(CV_KEY_DOWN, 0);
    CHECK_INT(g.log, CV_LOG_MENU);
    CHECK_INT(g.menu, 1);
    key(CV_KEY_DOWN, 0);
    CHECK_INT(g.log, CV_LOG_IDLE);
    key(CV_KEY_SELECT, 0);
    CHECK_INT(g.st, CV_ST_EXIT);
    CHECK(g.fx & CV_FX_EXIT);
    CHECK_INT(g.log, CV_LOG_EXIT);
}

TEST(culvert_title_back_quits)
{
    cv_game_init(&g, 0, 0);
    key(CV_KEY_BACK, 0);
    CHECK_INT(g.st, CV_ST_EXIT);
    CHECK(g.fx & CV_FX_EXIT);
}

TEST(culvert_title_to_play)
{
    start_fresh(1000);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.log, CV_LOG_LEVEL);
    CHECK_INT(g.lv.level, 1);
    CHECK_INT(g.lv.w, 5);
    CHECK_INT(g.lv.h, 4);
    CHECK_INT(g.cx, g.lv.sx);
    CHECK_INT(g.cy, g.lv.sy);
    CHECK_INT(g.moves, 0);
    CHECK(g.nconn >= 1);
    CHECK_INT(cv_game_wait(&g, 1000), 1000);
    CHECK_INT(cv_game_wait(&g, 1250), 750);
}

TEST(culvert_cursor_stops_at_edges)
{
    int i;

    start_fresh(0);
    for (i = 0; i < 12; i++) {
        ev(CV_EV_BUTTON, CV_KEY_LEFT, CV_REPEAT, 0);
    }
    CHECK_INT(g.cx, 0);
    CHECK_INT(g.log, CV_LOG_IDLE);
    for (i = 0; i < 12; i++) {
        ev(CV_EV_BUTTON, CV_KEY_DOWN, CV_REPEAT, 0);
    }
    CHECK_INT(g.cy, g.lv.h - 1);
    key(CV_KEY_UP, 0);
    CHECK_INT(g.log, CV_LOG_CURSOR);
    CHECK(g.fx & CV_FX_DRAW_CURSOR);
    CHECK_INT(g.px, 0);
    CHECK_INT(g.py, g.lv.h - 1);
    CHECK_INT(g.cy, g.lv.h - 2);
    ev(CV_EV_BUTTON, CV_KEY_UP, CV_RELEASE, 0);
    CHECK_INT(g.log, CV_LOG_NONE);
    CHECK_INT(g.fx, 0);
}

TEST(culvert_held_select_turns_once)
{
    uint8_t before;
    int i;

    start_fresh(0);
    before = g.lv.mask[g.cy * g.lv.w + g.cx];
    key(CV_KEY_SELECT, 0);
    CHECK_INT(g.log, CV_LOG_TURN);
    CHECK_INT(g.moves, 1);
    for (i = 0; i < 10; i++) {
        ev(CV_EV_BUTTON, CV_KEY_SELECT, CV_REPEAT, 0);
        CHECK_INT(g.log, CV_LOG_NONE);
    }
    ev(CV_EV_BUTTON, CV_KEY_SELECT, CV_RELEASE, 0);
    CHECK_INT(g.moves, 1);
    CHECK_INT(g.lv.mask[g.cy * g.lv.w + g.cx], cv_rot_cw(before));
}

TEST(culvert_dial_turns_both_ways)
{
    uint8_t before;

    start_fresh(0);
    before = g.lv.mask[g.cy * g.lv.w + g.cx];
    ev(CV_EV_DIAL, 0, 1, 0);
    CHECK_INT(g.lv.mask[g.cy * g.lv.w + g.cx], cv_rot_cw(before));
    ev(CV_EV_DIAL, 0, -2, 0);
    CHECK_INT(g.lv.mask[g.cy * g.lv.w + g.cx], cv_rot_ccw(before));
    CHECK_INT(g.moves, 3);
    CHECK_INT(g.log, CV_LOG_TURN);
    CHECK(g.fx & (CV_FX_TICK | CV_FX_CONNECT));
}

TEST(culvert_turn_sounds)
{
    start_fresh(0);
    /* Find a turn of the source that reaches a neighbour. */
    {
        int i, seen_connect = 0, seen_tick = 0;

        for (i = 0; i < 8; i++) {
            int before = g.nconn;

            key(CV_KEY_SELECT, 0);
            if (g.st != CV_ST_PLAY) {
                break;
            }
            if (g.nconn > before) {
                CHECK(g.fx & CV_FX_CONNECT);
                seen_connect = 1;
            } else {
                CHECK(g.fx & CV_FX_TICK);
                seen_tick = 1;
            }
        }
        CHECK(seen_tick || seen_connect);
    }
}

TEST(culvert_scripted_solve_and_next)
{
    uint32_t turns;

    start_fresh(100);
    turns = solve(100);
    CHECK_INT(g.st, CV_ST_SOLVED);
    CHECK_INT(g.log, CV_LOG_SOLVED);
    CHECK(g.fx & CV_FX_TUNE);
    CHECK(g.fx & CV_FX_SAVE);
    CHECK_INT(g.moves, turns);
    CHECK_INT(g.save.highest, 2);
    CHECK_INT(g.save.best[0], turns);
    CHECK_INT(g.nconn, g.lv.w * g.lv.h);
    /* SELECT waits for the flourish. */
    key(CV_KEY_SELECT, 200);
    CHECK_INT(g.st, CV_ST_SOLVED);
    CHECK_INT(g.log, CV_LOG_IDLE);
    CHECK_INT(cv_game_wait(&g, 100), CV_FLOURISH_MS);
    cv_game_tick(&g, 100 + CV_FLOURISH_MS);
    CHECK_INT(g.frame, 1);
    CHECK(g.fx & CV_FX_DRAW_GRID);
    cv_game_tick(&g, 100 + CV_FLOURISH_MS * CV_FLOURISH_FRAMES);
    CHECK_INT(g.frame, CV_FLOURISH_FRAMES);
    CHECK_INT(g.log, CV_LOG_DONE);
    CHECK(g.fx & CV_FX_DRAW_ALL);
    CHECK_INT(cv_game_wait(&g, 5000), CV_WAIT_FOREVER);
    /* The time stays at the solve. */
    CHECK_INT(cv_game_time(&g, 99999), 0);
    key(CV_KEY_SELECT, 5000);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.log, CV_LOG_LEVEL);
    CHECK_INT(g.lv.level, 2);
    CHECK_INT(g.moves, 0);
}

TEST(culvert_continue_from_save)
{
    uint8_t rec[CV_SAVE_SIZE];

    start_fresh(0);
    solve(0);
    cv_save_encode(&g.save, rec);
    cv_game_init(&g, rec, sizeof rec);
    CHECK_INT(cv_menu_count(&g), 3);
    CHECK_INT(cv_menu_item(&g, 0), CV_ITEM_CONTINUE);
    CHECK_INT(g.save.highest, 2);
    key(CV_KEY_SELECT, 0);
    CHECK_INT(g.lv.level, 2);
    /* New game starts at 1 and keeps the highest level. */
    cv_game_init(&g, rec, sizeof rec);
    key(CV_KEY_DOWN, 0);
    key(CV_KEY_SELECT, 0);
    CHECK_INT(g.lv.level, 1);
    CHECK_INT(g.save.highest, 2);
    /* A corrupt record starts fresh. */
    rec[3] ^= 1;
    cv_game_init(&g, rec, sizeof rec);
    CHECK_INT(cv_menu_count(&g), 2);
    CHECK_INT(g.save.highest, 1);
}

TEST(culvert_pause_resume_restart_quit)
{
    uint8_t first;

    start_fresh(0);
    first = g.lv.mask[g.cy * g.lv.w + g.cx];
    key(CV_KEY_SELECT, 0);
    key(CV_KEY_BACK, 3000);
    CHECK_INT(g.st, CV_ST_PAUSE);
    CHECK_INT(g.log, CV_LOG_PAUSE);
    CHECK_INT(cv_menu_count(&g), 3);
    CHECK_INT(cv_menu_item(&g, 0), CV_ITEM_RESUME);
    CHECK_INT(cv_menu_item(&g, 1), CV_ITEM_RESTART);
    CHECK_INT(cv_menu_item(&g, 2), CV_ITEM_QUIT);
    CHECK_INT(cv_game_wait(&g, 3000), CV_WAIT_FOREVER);
    /* The clock stops while paused. */
    CHECK_INT(cv_game_time(&g, 60000), 3000);
    key(CV_KEY_BACK, 60000);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.log, CV_LOG_RESUME);
    CHECK_INT(cv_game_time(&g, 61000), 4000);
    /* SELECT on Resume. */
    key(CV_KEY_BACK, 61000);
    key(CV_KEY_SELECT, 61000);
    CHECK_INT(g.st, CV_ST_PLAY);
    /* Restart: same level, no moves, the tile as generated. */
    key(CV_KEY_BACK, 62000);
    key(CV_KEY_DOWN, 62000);
    key(CV_KEY_SELECT, 62000);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.log, CV_LOG_LEVEL);
    CHECK_INT(g.moves, 0);
    CHECK_INT(g.lv.level, 1);
    CHECK_INT(g.lv.mask[g.cy * g.lv.w + g.cx], first);
    CHECK_INT(cv_game_time(&g, 62500), 500);
    /* Quit. */
    key(CV_KEY_BACK, 63000);
    key(CV_KEY_DOWN, 63000);
    key(CV_KEY_DOWN, 63000);
    key(CV_KEY_DOWN, 63000);
    CHECK_INT(g.log, CV_LOG_IDLE);
    key(CV_KEY_SELECT, 63000);
    CHECK_INT(g.st, CV_ST_EXIT);
    CHECK(g.fx & CV_FX_EXIT);
    CHECK_INT(g.log, CV_LOG_EXIT);
}

TEST(culvert_pause_while_solved)
{
    start_fresh(0);
    solve(0);
    key(CV_KEY_BACK, 50);
    CHECK_INT(g.st, CV_ST_PAUSE);
    key(CV_KEY_BACK, 60);
    CHECK_INT(g.st, CV_ST_SOLVED);
    CHECK_INT(g.frame, CV_FLOURISH_FRAMES);
    key(CV_KEY_SELECT, 70);
    CHECK_INT(g.lv.level, 2);
}

TEST(culvert_timer_across_wrap)
{
    uint32_t start = 0x7FFFFFFFu - 999u;

    start_fresh(start);
    CHECK_INT(cv_game_time(&g, start), 0);
    cv_game_tick(&g, 0x7FFFFFFFu);
    CHECK_INT(g.fx, 0);
    cv_game_tick(&g, 0);
    CHECK_INT(g.shown_s, 1);
    CHECK(g.fx & CV_FX_DRAW_STATUS);
    CHECK_INT(cv_game_time(&g, 1500), 2500);
    CHECK_INT(cv_game_wait(&g, 1500), 500);
    cv_game_tick(&g, 1500);
    CHECK_INT(g.shown_s, 2);
    /* A pause across the wrap. */
    key(CV_KEY_BACK, 2000);
    key(CV_KEY_BACK, 0x7FFFFF00u);
    CHECK_INT(cv_game_time(&g, 0x7FFFFF00u), 3000);
    CHECK_INT(cv_game_time(&g, 0x00000100u), 3000 + 0x200);
}

TEST(culvert_dial_push_is_select)
{
    uint8_t before;
    int i;

    /* Title: the push chooses New game. */
    cv_game_init(&g, 0, 0);
    key(CV_KEY_DIAL, 0);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.lv.level, 1);
    /* Play: the push turns once, and a held push does not repeat. */
    before = g.lv.mask[g.cy * g.lv.w + g.cx];
    key(CV_KEY_DIAL, 0);
    CHECK_INT(g.log, CV_LOG_TURN);
    CHECK_INT(g.moves, 1);
    for (i = 0; i < 10; i++) {
        ev(CV_EV_BUTTON, CV_KEY_DIAL, CV_REPEAT, 0);
        CHECK_INT(g.log, CV_LOG_NONE);
    }
    ev(CV_EV_BUTTON, CV_KEY_DIAL, CV_RELEASE, 0);
    CHECK_INT(g.moves, 1);
    CHECK_INT(g.lv.mask[g.cy * g.lv.w + g.cx], cv_rot_cw(before));
    /* Pause: the push picks Resume. */
    key(CV_KEY_BACK, 0);
    CHECK_INT(g.st, CV_ST_PAUSE);
    key(CV_KEY_DIAL, 0);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.log, CV_LOG_RESUME);
    /* Solved: the push starts the next level after the flourish. */
    solve(0);
    CHECK_INT(g.st, CV_ST_SOLVED);
    cv_game_tick(&g, CV_FLOURISH_MS * CV_FLOURISH_FRAMES);
    key(CV_KEY_DIAL, 5000);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.lv.level, 2);
}

/* Sets every tile to the solution, then puts the first end cap or tee
 * (four distinct orientations) `ahead` clockwise quarters before its
 * solution, with the cursor on it. Returns 0 if level 1 has no such tile. */
static int one_tile_left(int ahead)
{
    int i, n;

    start_fresh(0);
    n = g.lv.w * g.lv.h;
    for (i = 0; i < n; i++) {
        g.lv.mask[i] = g.lv.goal[i];
    }
    for (i = 0; i < n; i++) {
        int b = cv_bits(g.lv.goal[i]);

        if (b == 1 || b == 3) {
            g.lv.mask[i] = cv_rot_n(g.lv.goal[i], -ahead);
            go(i % g.lv.w, i / g.lv.w, 0);
            return 1;
        }
    }
    return 0;
}

static uint8_t here(void)
{
    return g.lv.mask[g.cy * g.lv.w + g.cx];
}

TEST(culvert_dial_two_detents_stop_at_solve)
{
    CHECK(one_tile_left(1));
    CHECK_INT(g.st, CV_ST_PLAY);
    ev(CV_EV_DIAL, 0, 2, 0);
    CHECK_INT(g.st, CV_ST_SOLVED);
    CHECK_INT(g.log, CV_LOG_SOLVED);
    CHECK_INT(g.moves, 1);
    CHECK_INT(here(), g.lv.goal[g.cy * g.lv.w + g.cx]);
}

TEST(culvert_dial_three_detents_stop_at_solve)
{
    CHECK(one_tile_left(2));
    ev(CV_EV_DIAL, 0, 3, 0);
    CHECK_INT(g.st, CV_ST_SOLVED);
    CHECK_INT(g.moves, 2);
    CHECK_INT(here(), g.lv.goal[g.cy * g.lv.w + g.cx]);
    /* The same counter-clockwise. */
    CHECK(one_tile_left(-1));
    ev(CV_EV_DIAL, 0, -3, 0);
    CHECK_INT(g.st, CV_ST_SOLVED);
    CHECK_INT(g.moves, 1);
}

TEST(culvert_dial_spin_applies_count_mod_4)
{
    uint8_t before;

    /* 3 quarters before the solution: 5 detents apply 1 quarter only. */
    CHECK(one_tile_left(3));
    before = here();
    ev(CV_EV_DIAL, 0, 5, 0);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.moves, 1);
    CHECK_INT(here(), cv_rot_cw(before));
    /* A full rotation does nothing and counts no move. */
    ev(CV_EV_DIAL, 0, 8, 0);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.log, CV_LOG_IDLE);
    CHECK_INT(g.moves, 1);
    CHECK_INT(here(), cv_rot_cw(before));
    /* 2 before now. -4003 detents are 3 counter-clockwise quarters; the
     * second reaches the solution and the third is dropped. */
    ev(CV_EV_DIAL, 0, -4003, 0);
    CHECK_INT(g.st, CV_ST_SOLVED);
    CHECK_INT(g.moves, 3);
    CHECK_INT(here(), g.lv.goal[g.cy * g.lv.w + g.cx]);
}

TEST(culvert_dial_multi_detent_without_solve)
{
    uint8_t before;

    start_fresh(0);
    before = here();
    ev(CV_EV_DIAL, 0, 3, 0);
    CHECK_INT(g.st, CV_ST_PLAY);
    CHECK_INT(g.moves, 3);
    CHECK_INT(here(), cv_rot_n(before, 3));
    ev(CV_EV_DIAL, 0, -2, 0);
    CHECK_INT(g.moves, 5);
    CHECK_INT(here(), cv_rot_n(before, 1));
}

/* Expected bar after a dial event of `v` steps from item `from` in a menu
 * of `n` items: one item per step, stopped at both ends. */
static int dial_rule(int from, int32_t v, int n)
{
    int64_t m = (int64_t)from + v;

    return m < 0 ? 0 : (m >= n ? n - 1 : (int)m);
}

static void check_menu_dial(int st, int n)
{
    static const int32_t vals[] = { 1, 2, 3, 7, 0x7FFFFFFF, -1, -2, -3, -7,
                                    (int32_t)0x80000000u };
    unsigned i;
    int from;

    for (from = 0; from < n; from++) {
        for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
            int want = dial_rule(from, vals[i], n);

            CHECK_INT(g.st, st);
            CHECK_INT(cv_menu_count(&g), n);
            g.menu = from;
            ev(CV_EV_DIAL, 0, vals[i], 0);
            CHECK_INT(g.st, st);
            CHECK_INT(g.menu, want);
            CHECK_INT(g.log, want == from ? CV_LOG_IDLE : CV_LOG_MENU);
        }
        g.menu = from;
        ev(CV_EV_DIAL, 0, 0, 0);
        CHECK_INT(g.menu, from);
        CHECK_INT(g.log, CV_LOG_IDLE);
        CHECK_INT(g.fx, 0);
    }
}

TEST(culvert_dial_menu_moves_by_steps)
{
    /* Title of a fresh save: New game, Quit. */
    cv_game_init(&g, 0, 0);
    check_menu_dial(CV_ST_TITLE, 2);
    /* Title with Continue: three items. */
    cv_game_init(&g, 0, 0);
    g.save.highest = 2;
    check_menu_dial(CV_ST_TITLE, 3);
    /* Two steps from the top reach Quit. */
    g.menu = 0;
    ev(CV_EV_DIAL, 0, 2, 0);
    CHECK_INT(cv_menu_item(&g, g.menu), CV_ITEM_QUIT);
    /* The pause menu. */
    start_fresh(0);
    key(CV_KEY_BACK, 0);
    check_menu_dial(CV_ST_PAUSE, 3);
    g.menu = 0;
    ev(CV_EV_DIAL, 0, 2, 0);
    CHECK_INT(cv_menu_item(&g, g.menu), CV_ITEM_QUIT);
    key(CV_KEY_DIAL, 0);
    CHECK_INT(g.st, CV_ST_EXIT);
}
