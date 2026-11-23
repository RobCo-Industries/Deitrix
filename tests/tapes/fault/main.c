/*
 * A test holotape that faults on purpose. It draws a little, so the
 * ROM must redraw after it, then runs an undefined instruction.
 */
#include <pipos/pip.h>

int pip_main(const pipos_startup_t *startup)
{
    (void)startup;
    pip_clear(0);
    pip_set_color(0xffffff, PIP_COLOR_NONE);
    pip_rect_xywh(40, 40, 240, 160, PIP_RECT_FILLED);
    pip_present();
    pip_printf("faulttest: fault now\n");
    __builtin_trap();
    return 0;
}
