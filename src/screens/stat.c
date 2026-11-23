/* The STAT tab: the status screen. A placeholder for now. */
#include "screens/screens.h"

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    (void)st;
    dx_placeholder_draw(ctx, area, "STAT");
}

const dx_screen_t dx_screen_stat = {
    "STAT", NULL, NULL, NULL, NULL, draw, NULL, NULL,
};
