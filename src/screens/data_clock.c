/* The DATA tab's CLOCK sub-tab. Not built yet. */
#include "screens/data_view.h"

void dx_data_clock_reset(void) {}
void dx_data_clock_leave(void) {}
int dx_data_clock_nested(void)
{
    return 0;
}
unsigned dx_data_clock_event(const dx_ui_event_t *ev)
{
    (void)ev;
    return DX_UI_IGNORED;
}
uint32_t dx_data_clock_tick(uint32_t now_ms, unsigned *flags)
{
    (void)now_ms;
    (void)flags;
    return 0;
}
void dx_data_clock_draw(const dx_ctx_t *ctx, dx_rect_t r)
{
    dx_data_message(ctx, r, "Not built yet.", NULL);
}
const char *dx_data_clock_hint(void)
{
    return "LEFT: TAPES   RIGHT or DIAL: tab";
}
