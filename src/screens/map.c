/* The MAP tab: the map screen. A placeholder for now. */
#include "screens/screens.h"

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    (void)st;
    dx_placeholder_draw(ctx, area, "MAP");
}

const dx_screen_t dx_screen_map = {
    "MAP", NULL, NULL, NULL, NULL, draw, NULL, NULL,
};
