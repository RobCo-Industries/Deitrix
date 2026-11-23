/*
 * A test holotape for the DATA tab. It draws a frame, a line and its
 * name, logs "linetest: ready", waits for SELECT and exits with 42.
 */
#include <pipos/pip.h>

#define STATUS 42

int pip_main(const pipos_startup_t *startup)
{
    pip_gfx_info_t gi;
    pip_event_t ev;
    uint32_t fg = pip_gfx_info(&gi) == 0 ? gi.fg_default : 0x33e066;

    pip_clear(0);
    pip_set_color(fg, PIP_COLOR_NONE);
    pip_rect_xywh(0, 0, 320, 240, PIP_RECT_OUTLINE);
    pip_draw_line(20, 120, 300, 120);
    pip_draw_text(112, 40, "LINE TEST", 1);
    pip_draw_text(84, 160, "SELECT: back to ROM", 1);
    pip_present();
    pip_printf("linetest: ready args \"%s\"\n", startup->args);
    for (;;) {
        if (pip_wait_event(&ev, PIP_WAIT_FOREVER) == 1 && ev.type == PIP_EV_BUTTON &&
            ev.value == PIP_EV_PRESS && ev.code == PIP_BTN_SELECT) {
            break;
        }
    }
    pip_printf("linetest: exit %d\n", STATUS);
    return STATUS;
}
