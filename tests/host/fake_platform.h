/*
 * A fake platform layer for the host tests. It records draw calls and
 * log lines, and plays back scripted events and clock readings.
 */
#ifndef DX_FAKE_PLATFORM_H
#define DX_FAKE_PLATFORM_H

#include <stdint.h>

#include "platform/platform.h"

#define FAKE_MAX_TEXTS 64
#define FAKE_MAX_RECTS 16
#define FAKE_LOG_SIZE  1024
#define FAKE_MAX_EVENTS 16

typedef struct fake_text {
    int32_t x, y;
    uint32_t scale;
    uint32_t fg, bg;
    char text[40];
} fake_text_t;

typedef struct fake_rect {
    int32_t x, y;
    uint32_t w, h;
    int filled;
    uint32_t fg;
} fake_rect_t;

typedef struct fake_platform {
    /* Inputs the test sets. */
    uint32_t fg;
    int clock_valid;
    uint32_t clock;
    dx_event_t events[FAKE_MAX_EVENTS];
    int event_count;
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
    char log[FAKE_LOG_SIZE];
} fake_platform_t;

extern fake_platform_t fake;

/* Clears every field, with fg 0x33e066 and no clock. */
void fake_reset(void);

/* Queues one event for dx_plat_wait. */
void fake_push(uint32_t type, uint32_t code, int32_t value);

/* Forgets the draw calls so far, keeps the inputs and the log. */
void fake_clear_draws(void);

/* The recorded text call with this text, or NULL. */
const fake_text_t *fake_find_text(const char *text);

#endif
