/* The STAT tab: the status screen. A placeholder for now, with the owner name. */
#include "screens/screens.h"

#include "content.h"

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_profile_t *p = dx_content_profile();
    char buf[40];

    (void)st;
    dx_placeholder_draw(ctx, area, "STAT");
    if (p->load.status != DX_LOAD_OK) {
        dx_placeholder_lines(ctx, area, dx_placeholder_reason(&p->load, "PROFILE.INI", buf,
                                                              sizeof(buf)), NULL);
    } else {
        dx_placeholder_lines(ctx, area, p->name[0] != '\0' ? p->name : NULL, NULL);
    }
}

const dx_screen_t dx_screen_stat = {
    "STAT", NULL, NULL, NULL, NULL, draw, NULL, NULL,
};
