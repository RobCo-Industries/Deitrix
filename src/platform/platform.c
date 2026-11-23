/* The platform layer on Pip-OS: the only file that makes system calls. */
#include <pipos/pip.h>

#include "app.h"
#include "platform/platform.h"

/* The fallback when gfx_info fails, which Pip-OS does not do today. */
#define FALLBACK_FG 0x33e066u

_Static_assert(DX_EV_BUTTON == PIP_EV_BUTTON && DX_EV_DIAL == PIP_EV_DIAL, "event types");
_Static_assert(DX_BTN_LEFT == PIP_BTN_LEFT && DX_BTN_RIGHT == PIP_BTN_RIGHT, "buttons");
_Static_assert(DX_BTN_SELECT == PIP_BTN_SELECT && DX_BTN_DIAL == PIP_BTN_DIAL, "buttons");
_Static_assert(DX_BTN_BACK == PIP_BTN_BACK, "buttons");
_Static_assert(DX_PRESS_RELEASE == PIP_EV_RELEASE && DX_PRESS_REPEAT == PIP_EV_REPEAT, "press");
_Static_assert(DX_FONT_W == PIP_FONT_W && DX_FONT_H == PIP_FONT_H, "font");
_Static_assert(DX_COLOR_NONE == PIP_COLOR_NONE, "colour none");
_Static_assert(DX_SCALE_MAX == PIP_SCALE_MAX, "text scale");

uint32_t dx_plat_fg(void)
{
    pip_gfx_info_t gi;

    return pip_gfx_info(&gi) == 0 ? gi.fg_default : FALLBACK_FG;
}

int dx_plat_time(uint32_t *unix_seconds)
{
    return pip_get_time(unix_seconds) == 0;
}

uint32_t dx_plat_ticks(void)
{
    return (uint32_t)pip_ticks_ms();
}

int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms)
{
    pip_event_t pe;

    if (timeout_ms != DX_WAIT_FOREVER && timeout_ms > PIP_WAIT_MAX_MS) {
        timeout_ms = PIP_WAIT_MAX_MS;
    }
    if (pip_wait_event(&pe, timeout_ms) != 1) {
        return 0;
    }
    ev->type = pe.type;
    ev->code = pe.code;
    ev->value = pe.value;
    return 1;
}

void dx_plat_log(const char *text)
{
    pip_log(text);
}

void dx_plat_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    pip_tone(freq_hz, duration_ms);
}

/* ---- Drawing ------------------------------------------------------------ */

static void g_clear(uint32_t color)
{
    pip_clear(color);
}

static void g_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    pip_set_color(color, PIP_COLOR_NONE);
    pip_draw_line(x0, y0, x1, y1);
}

static void g_rect(int x, int y, int w, int h, uint32_t color, int fill)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    pip_set_color(color, PIP_COLOR_NONE);
    pip_rect_xywh(x, y, (uint32_t)w, (uint32_t)h, fill ? PIP_RECT_FILLED : PIP_RECT_OUTLINE);
}

static void g_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale)
{
    if (scale < PIP_SCALE_MIN) {
        scale = PIP_SCALE_MIN;
    } else if (scale > PIP_SCALE_MAX) {
        scale = PIP_SCALE_MAX;
    }
    pip_set_color(fg, bg);
    pip_draw_text(x, y, s, (uint32_t)scale);
}

static void g_polyline(const int16_t *xy, int points, int closed, uint32_t color)
{
    if (points < PIP_POLY_MIN || points > DX_POLY_MAX) {
        return;
    }
    pip_set_color(color, PIP_COLOR_NONE);
    pip_polyline(xy, (uint32_t)points, closed ? 1u : 0u);
}

static void g_arc(int cx, int cy, int r, int start_deg, int end_deg, uint32_t color, int fill)
{
    pip_arc_t a;

    if (r < PIP_ARC_RADIUS_MIN || r > PIP_ARC_RADIUS_MAX || start_deg < 0 ||
        start_deg > PIP_ARC_START_MAX || end_deg < 0 || end_deg > PIP_ARC_END_MAX) {
        return;
    }
    a.cx = (int16_t)cx;
    a.cy = (int16_t)cy;
    a.radius = (uint16_t)r;
    a.start_deg = (uint16_t)start_deg;
    a.end_deg = (uint16_t)end_deg;
    a.fill = fill ? PIP_ARC_FILLED : PIP_ARC_OUTLINE;
    pip_set_color(color, PIP_COLOR_NONE);
    pip_arc(&a);
}

static void g_blit1(int x, int y, int w, int h, const uint8_t *bits, int stride, uint32_t fg,
                    uint32_t bg)
{
    pip_blit_t b;

    if (w < 1 || w > PIP_BLIT_W_MAX || h < 1 || h > PIP_BLIT_H_MAX) {
        return;
    }
    /* blit_1bpp draws set bits only: the background is a rectangle first. */
    if (bg != PIP_COLOR_NONE) {
        g_rect(x, y, w, h, bg, 1);
    }
    b.x = (int16_t)x;
    b.y = (int16_t)y;
    b.w = (uint16_t)w;
    b.h = (uint16_t)h;
    b.stride = (uint16_t)stride;
    b.flags = 0;
    b.data = (uint32_t)(uintptr_t)bits;
    b.color = fg;
    pip_blit_1bpp(&b);
}

static void g_clip(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) {
        pip_set_clip(0, 0, 0);
    } else {
        pip_set_clip_xywh(x, y, (uint32_t)w, (uint32_t)h);
    }
}

static void g_present(void)
{
    pip_present();
}

static const dx_gfx_t gfx = {
    g_clear, g_line, g_rect, g_text, g_polyline, g_arc, g_blit1, g_clip, g_present,
};

const dx_gfx_t *dx_plat_gfx(void)
{
    return &gfx;
}

int pip_main(const pipos_startup_t *startup)
{
    (void)startup;
    dx_main();
}
