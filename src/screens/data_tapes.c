/* The DATA tab's TAPES sub-tab. Not built yet. */
#include "screens/data_view.h"

void dx_data_tapes_reset(void) {}
void dx_data_tapes_enter(void) {}
void dx_data_tapes_leave(void) {}
unsigned dx_data_tapes_event(const dx_ui_event_t *ev)
{
    (void)ev;
    return DX_UI_IGNORED;
}
void dx_data_tapes_draw(const dx_ctx_t *ctx, dx_rect_t r)
{
    dx_data_message(ctx, r, "Not built yet.", NULL);
}
const char *dx_data_tapes_hint(void)
{
    return "LEFT/RIGHT: page   DIAL: tab";
}
