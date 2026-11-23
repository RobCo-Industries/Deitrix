/*
 * A fake platform layer for the host tests. Its drawing table records
 * every call as one line of a draw list, plus the text and rectangle
 * calls as structs. It also records log lines and tones, and plays back
 * scripted events, clock readings and ticks.
 */
#ifndef DX_FAKE_PLATFORM_H
#define DX_FAKE_PLATFORM_H

#include <stdint.h>

#include "platform/platform.h"
#include "ui/ui.h"

#define FAKE_MAX_TEXTS  128
#define FAKE_MAX_RECTS  64
#define FAKE_LOG_SIZE   2048
#define FAKE_MAX_EVENTS 64
#define FAKE_DRAW_SIZE  16384
#define FAKE_MAX_TONES  32
#define FAKE_MAX_POINTS 512

typedef struct fake_text {
    int32_t x, y;
    int scale;
    uint32_t fg, bg;
    char text[48];
} fake_text_t;

typedef struct fake_rect {
    int32_t x, y, w, h;
    int filled;
    uint32_t fg;
} fake_rect_t;

/* A point any call touched, for the inside-the-area checks. */
typedef struct fake_point {
    int x, y;
} fake_point_t;

typedef struct fake_platform {
    /* Inputs the test sets. */
    uint32_t fg;
    int clock_valid;
    uint32_t clock;
    uint32_t ticks;
    uint32_t tick_step; /* added to ticks at each wait */
    dx_event_t events[FAKE_MAX_EVENTS];
    int event_count;
    /* The theme the draw list names colours by. */
    dx_theme_t theme;
    /* What the code under test did. */
    int event_next;
    uint32_t last_timeout;
    int waits;
    int presents;
    int clears;
    fake_text_t texts[FAKE_MAX_TEXTS];
    int text_count;
    fake_rect_t rects[FAKE_MAX_RECTS];
    int rect_count;
    /* Corners of rects, ends of lines, text boxes, blits. */
    fake_point_t points[FAKE_MAX_POINTS];
    int point_count;
    char draws[FAKE_DRAW_SIZE];
    char log[FAKE_LOG_SIZE];
    uint32_t tones[FAKE_MAX_TONES][2];
    int tone_count;
} fake_platform_t;

extern fake_platform_t fake;

/* Clears every field, with fg 0x33e066, its theme and no clock. */
void fake_reset(void);

/* Queues one event for dx_plat_wait. */
void fake_push(uint32_t type, uint32_t code, int32_t value);

/* Forgets the draw calls so far, keeps the inputs and the log. */
void fake_clear_draws(void);

/* The recorded text call with this text, or NULL. */
const fake_text_t *fake_find_text(const char *text);

/* A context that draws into the recorder with fake.theme. */
dx_ctx_t fake_ctx(void);

/* 1 when every recorded point lies inside r. */
int fake_points_inside(dx_rect_t r);

#endif
