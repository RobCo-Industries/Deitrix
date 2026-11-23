/* The platform layer on Pip-OS: the only file that makes system calls. */
#include <pipos/pip.h>

#include "app.h"
#include "platform/platform.h"

/* The fallback when gfx_info fails, which Pip-OS does not do today. */
#define FALLBACK_FG 0x33e066u

_Static_assert(DX_EV_BUTTON == PIP_EV_BUTTON && DX_EV_DIAL == PIP_EV_DIAL, "event types");
_Static_assert(DX_BTN_LEFT == PIP_BTN_LEFT && DX_BTN_RIGHT == PIP_BTN_RIGHT, "buttons");
_Static_assert(DX_PRESS_RELEASE == PIP_EV_RELEASE && DX_PRESS_REPEAT == PIP_EV_REPEAT, "press");
_Static_assert(DX_FONT_W == PIP_FONT_W && DX_FONT_H == PIP_FONT_H, "font");

uint32_t dx_plat_fg(void)
{
    pip_gfx_info_t gi;

    return pip_gfx_info(&gi) == 0 ? gi.fg_default : FALLBACK_FG;
}

int dx_plat_time(uint32_t *unix_seconds)
{
    return pip_get_time(unix_seconds) == 0;
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

void dx_plat_clear(uint32_t color)
{
    pip_clear(color);
}

void dx_plat_color(uint32_t fg, uint32_t bg)
{
    pip_set_color(fg, bg);
}

void dx_plat_text(int32_t x, int32_t y, const char *text, uint32_t scale)
{
    pip_draw_text(x, y, text, scale);
}

void dx_plat_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, int filled)
{
    pip_rect_xywh(x, y, w, h, filled ? PIP_RECT_FILLED : PIP_RECT_OUTLINE);
}

void dx_plat_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    pip_draw_line(x0, y0, x1, y1);
}

void dx_plat_present(void)
{
    pip_present();
}

int pip_main(const pipos_startup_t *startup)
{
    (void)startup;
    dx_main();
}
