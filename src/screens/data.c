/* The DATA tab: the notes and data screen. A placeholder for now. */
#include "screens/screens.h"

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    (void)st;
    dx_placeholder_draw(ctx, area, "DATA");
}

const dx_screen_t dx_screen_data = {
    "DATA", NULL, NULL, NULL, NULL, draw, NULL, NULL,
};
