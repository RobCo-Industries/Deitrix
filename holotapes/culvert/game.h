/*
 * Culvert's state machine: title, play, pause, solved. Pure: it takes
 * input events and the tick, and says what to draw, play, log and save
 * through the effect bits. main.c does the kernel calls.
 */
#ifndef CV_GAME_H
#define CV_GAME_H

#include <stdint.h>

#include "logic.h"

/* Input, with the kernel's numbers. */
#define CV_EV_BUTTON  1
#define CV_EV_DIAL    2
#define CV_RELEASE    0
#define CV_PRESS      1
#define CV_REPEAT     2
#define CV_KEY_UP     0
#define CV_KEY_DOWN   1
#define CV_KEY_LEFT   2
#define CV_KEY_RIGHT  3
#define CV_KEY_SELECT 4
#define CV_KEY_DIAL   5
#define CV_KEY_BACK   6

typedef struct {
    uint32_t type;
    uint32_t code;
    int32_t value;
} cv_input_t;

typedef enum {
    CV_ST_TITLE,
    CV_ST_PLAY,
    CV_ST_PAUSE,
    CV_ST_SOLVED,
    CV_ST_EXIT,
} cv_state_t;

/* Effects of one step, for main.c. */
#define CV_FX_DRAW_ALL    0x001u    /* the whole screen */
#define CV_FX_DRAW_GRID   0x002u    /* the grid and the status row */
#define CV_FX_DRAW_CURSOR 0x004u    /* the old and the new cursor tile */
#define CV_FX_DRAW_STATUS 0x008u    /* the status row */
#define CV_FX_TICK        0x010u    /* a turn that connected nothing new */
#define CV_FX_CONNECT     0x020u    /* a turn that connected new tiles */
#define CV_FX_TUNE        0x040u    /* the level is solved */
#define CV_FX_SAVE        0x080u    /* write the save */
#define CV_FX_EXIT        0x100u    /* leave with status 0 */

/* The one log line of a step. */
typedef enum {
    CV_LOG_NONE,
    CV_LOG_TITLE,       /* culvert: title */
    CV_LOG_LEVEL,       /* culvert: level n= w= h= seed= */
    CV_LOG_TURN,        /* culvert: turn x= y= connected= */
    CV_LOG_CURSOR,      /* culvert: cursor x= y= */
    CV_LOG_MENU,        /* culvert: menu item= */
    CV_LOG_PAUSE,       /* culvert: pause */
    CV_LOG_RESUME,      /* culvert: resume */
    CV_LOG_SOLVED,      /* culvert: solved level= moves= */
    CV_LOG_DONE,        /* culvert: done, the message is on screen */
    CV_LOG_IDLE,        /* culvert: idle, a key that changed nothing */
    CV_LOG_EXIT,        /* culvert: exit rc=0 */
} cv_log_t;

#define CV_FLOURISH_FRAMES 6
#define CV_FLOURISH_MS     120u
#define CV_WAIT_FOREVER    0xFFFFFFFFu

/* Menu entries. */
typedef enum {
    CV_ITEM_CONTINUE,
    CV_ITEM_NEW,
    CV_ITEM_QUIT,
    CV_ITEM_RESUME,
    CV_ITEM_RESTART,
} cv_item_t;

typedef struct {
    cv_state_t st;
    cv_state_t paused_from;     /* PLAY or SOLVED */
    cv_level_t lv;
    uint8_t conn[CV_MAX_TILES];
    int nconn;
    int cx, cy;                 /* the cursor */
    int px, py;                 /* the cursor before this step */
    int tx, ty;                 /* the tile turned in this step */
    uint32_t moves;
    uint32_t start;             /* tick at the level start, moved on by pauses */
    uint32_t frozen;            /* the time shown while paused or solved */
    uint32_t shown_s;           /* the second on screen */
    uint32_t solved_at;
    int frame;                  /* flourish frame, CV_FLOURISH_FRAMES when done */
    int menu;                   /* the selected entry */
    cv_save_t save;
    uint32_t fx;
    cv_log_t log;
} cv_game_t;

/* Starts on the title with the save bytes as read, or len 0. */
void cv_game_init(cv_game_t *g, const uint8_t *save, uint32_t len);
void cv_game_event(cv_game_t *g, const cv_input_t *ev, uint32_t now);
void cv_game_tick(cv_game_t *g, uint32_t now);
/* Milliseconds until the next tick matters, or CV_WAIT_FOREVER. */
uint32_t cv_game_wait(const cv_game_t *g, uint32_t now);
/* The time to show, in ms. */
uint32_t cv_game_time(const cv_game_t *g, uint32_t now);

/* The menu of the current screen: title or pause. */
int cv_menu_count(const cv_game_t *g);
cv_item_t cv_menu_item(const cv_game_t *g, int i);

#endif
