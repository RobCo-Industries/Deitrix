/*
 * The platform layer: the only interface to Pip-OS. platform.c calls the
 * pip_* system calls. Everything else uses these functions, so the host
 * tests can link a fake instead (tests/host/fake_platform.c).
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
#define DX_WAIT_FOREVER 0xFFFFFFFFu
#define DX_COLOR_NONE   0x01000000u /* a transparent background */

/* The foreground colour of the card's screen scheme, 0x00RRGGBB. */
uint32_t dx_plat_fg(void);

/* 1 and the Unix seconds, or 0 when Pip-OS has no clock. */
int dx_plat_time(uint32_t *unix_seconds);

/* 1 with an event, 0 when timeout_ms passed first. */
int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms);

/* Writes text to the serial log as it is. */
void dx_plat_log(const char *text);

void dx_plat_clear(uint32_t color);
void dx_plat_color(uint32_t fg, uint32_t bg);
void dx_plat_text(int32_t x, int32_t y, const char *text, uint32_t scale);
void dx_plat_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, int filled);
void dx_plat_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
void dx_plat_present(void);

#endif
