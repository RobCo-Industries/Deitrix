/*
 * The platform layer: the only interface to Pip-OS. platform.c calls the
 * pip_* system calls. Everything else uses these functions and the
 * drawing table, so the host tests can link a fake instead
 * (tests/host/fake_platform.c).
 */
#ifndef DX_PLATFORM_H
#define DX_PLATFORM_H

#include <stdint.h>

/* Event types, button codes and press values, as in Pip-OS's ABI. */
enum { DX_EV_NONE = 0, DX_EV_BUTTON = 1, DX_EV_DIAL = 2 };
enum { DX_BTN_UP, DX_BTN_DOWN, DX_BTN_LEFT, DX_BTN_RIGHT, DX_BTN_SELECT, DX_BTN_DIAL };
enum { DX_PRESS_RELEASE = 0, DX_PRESS_DOWN = 1, DX_PRESS_REPEAT = 2 };

typedef struct dx_event {
    uint32_t type;  /* DX_EV_* */
    uint32_t code;  /* DX_BTN_*, or 0 for the dial */
    int32_t value;  /* DX_PRESS_*, or signed dial detents */
} dx_event_t;

#define DX_SCREEN_W 320
#define DX_SCREEN_H 240
#define DX_FONT_W   8
#define DX_FONT_H   16
#define DX_SCALE_MAX 8
#define DX_POLY_MAX 256 /* points in one polyline */
#define DX_WAIT_FOREVER 0xFFFFFFFFu
#define DX_COLOR_NONE   0x01000000u /* a transparent background */

/*
 * The drawing calls. Colours are 0x00RRGGBB. A text background of
 * DX_COLOR_NONE is transparent. Every call clips to the clip rectangle
 * and to the screen.
 */
typedef struct dx_gfx {
    void (*clear)(uint32_t color);
    void (*line)(int x0, int y0, int x1, int y1, uint32_t color);
    void (*rect)(int x, int y, int w, int h, uint32_t color, int fill);
    /* Scale 1 to DX_SCALE_MAX. */
    void (*text)(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale);
    /* xy holds 2 to DX_POLY_MAX points, x then y. */
    void (*polyline)(const int16_t *xy, int points, int closed, uint32_t color);
    /* Degrees clockwise from +x, start 0 to 359, end 0 to 360. */
    void (*arc)(int cx, int cy, int r, int start_deg, int end_deg, uint32_t color, int fill);
    /* Rows packed most significant bit first. Set bits draw fg, clear bits bg. */
    void (*blit1)(int x, int y, int w, int h, const uint8_t *bits, int stride,
                  uint32_t fg, uint32_t bg);
    /* w or h 0: no clip. */
    void (*clip)(int x, int y, int w, int h);
    void (*present)(void);
} dx_gfx_t;

/* The drawing calls of this platform. */
const dx_gfx_t *dx_plat_gfx(void);

/* The foreground colour of the card's screen scheme, 0x00RRGGBB. */
uint32_t dx_plat_fg(void);

/* 1 and the Unix seconds, or 0 when Pip-OS has no clock. */
int dx_plat_time(uint32_t *unix_seconds);

/* Milliseconds since boot. Wraps. */
uint32_t dx_plat_ticks(void);

/* 1 with an event, 0 when timeout_ms passed first. */
int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms);

/* Writes text to the serial log as it is. */
void dx_plat_log(const char *text);

/* Starts a square-wave tone. It plays in the background. */
void dx_plat_tone(uint32_t freq_hz, uint32_t duration_ms);

#endif
