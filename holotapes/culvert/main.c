/*
 * Culvert, a pipe-turning puzzle holotape. The event loop and every
 * kernel call except drawing. game.c holds the rules, draw.c the screens.
 */
#include <pipos/pip.h>

#include "draw.h"
#include "game.h"

static cv_game_t game;

static const pip_note_t connect_notes[] = { { 1320, 30 }, { 1760, 40 } };
static const pip_note_t solved_tune[] = {
    { 784, 90 }, { 988, 90 }, { 1175, 90 }, { PIP_NOTE_REST, 40 }, { 1047, 120 }, { 1568, 220 },
};

/* The full grid redraws and their time, for the redraw cost. */
static uint32_t grid_draws, grid_ms;

static void log_step(const cv_game_t *g)
{
    switch (g->log) {
    case CV_LOG_TITLE:
        pip_printf("culvert: title\n");
        break;
    case CV_LOG_LEVEL:
        pip_printf("culvert: level n=%u w=%u h=%u seed=0x%08x\n", (unsigned)g->lv.level,
                   (unsigned)g->lv.w, (unsigned)g->lv.h, (unsigned)g->lv.seed);
        break;
    case CV_LOG_TURN:
        pip_printf("culvert: turn x=%d y=%d connected=%d\n", g->tx, g->ty, g->nconn);
        break;
    case CV_LOG_CURSOR:
        pip_printf("culvert: cursor x=%d y=%d\n", g->cx, g->cy);
        break;
    case CV_LOG_MENU:
        pip_printf("culvert: menu item=%d\n", g->menu);
        break;
    case CV_LOG_PAUSE:
        pip_printf("culvert: pause\n");
        break;
    case CV_LOG_RESUME:
        pip_printf("culvert: resume\n");
        break;
    case CV_LOG_SOLVED:
        pip_printf("culvert: solved level=%u moves=%u\n", (unsigned)g->lv.level,
                   (unsigned)g->moves);
        break;
    case CV_LOG_DONE:
        pip_printf("culvert: done\n");
        break;
    case CV_LOG_IDLE:
        pip_printf("culvert: idle\n");
        break;
    case CV_LOG_EXIT:
        pip_printf("culvert stats: grid draws=%u ms=%u\n", (unsigned)grid_draws, (unsigned)grid_ms);
        pip_printf("culvert: exit rc=0\n");
        break;
    default:
        break;
    }
}

static void save(const cv_game_t *g)
{
    uint8_t rec[CV_SAVE_SIZE];
    int32_t rc;

    cv_save_encode(&g->save, rec);
    rc = pip_write_save(CV_SAVE_NAME, rec, sizeof rec);
    pip_printf("culvert: save rc=%d\n", (int)rc);
}

/* Draws, logs, plays and saves one step. The log line comes after the
 * frame is on screen, so a test that waits for it sees the frame. */
static void apply(const cv_game_t *g)
{
    uint32_t t0 = pip_ticks_ms();

    cv_draw(g, t0);
    if (g->fx & (CV_FX_DRAW_ALL | CV_FX_DRAW_GRID) && g->st != CV_ST_TITLE &&
        g->st != CV_ST_EXIT) {
        grid_draws++;
        grid_ms += cv_elapsed(t0, pip_ticks_ms());
    }
    log_step(g);
    if (g->fx & CV_FX_TUNE) {
        pip_play_seq(solved_tune, sizeof solved_tune / sizeof solved_tune[0]);
    } else if (g->fx & CV_FX_CONNECT) {
        pip_play_seq(connect_notes, sizeof connect_notes / sizeof connect_notes[0]);
    } else if (g->fx & CV_FX_TICK) {
        pip_tone(900, 12);
    }
    if (g->fx & CV_FX_SAVE) {
        save(g);
    }
}

int pip_main(const pipos_startup_t *startup)
{
    static uint8_t rec[CV_SAVE_SIZE + 1];
    int32_t n;

    (void)startup;
    n = pip_read_save(CV_SAVE_NAME, rec, sizeof rec);
    pip_printf("culvert: load rc=%d\n", (int)n);
    cv_draw_init();
    cv_game_init(&game, rec, n > 0 ? (uint32_t)n : 0);
    apply(&game);
    while (game.st != CV_ST_EXIT) {
        pip_event_t ev;
        uint32_t wait = cv_game_wait(&game, pip_ticks_ms());
        int32_t rc = pip_wait_event(&ev, wait == CV_WAIT_FOREVER ? PIP_WAIT_FOREVER : wait);

        if (rc == 1) {
            cv_input_t in = { ev.type, ev.code, ev.value };

            cv_game_event(&game, &in, pip_ticks_ms());
            apply(&game);
            if (game.st == CV_ST_EXIT) {
                break;
            }
        }
        cv_game_tick(&game, pip_ticks_ms());
        apply(&game);
    }
    pip_audio_stop();
    return 0;
}
