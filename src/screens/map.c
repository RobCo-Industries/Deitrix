/* The MAP tab: the map screen. A placeholder for now, with the first map. */
#include "screens/screens.h"

#include "content.h"

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_maps_t *m = dx_content_maps();
    char buf[48];
    dx_sb_t sb;

    (void)st;
    dx_placeholder_draw(ctx, area, "MAP");
    if (m->load.status != DX_LOAD_OK) {
        dx_placeholder_reason(&m->load, "MAP.INI", buf, sizeof(buf));
    } else if (m->count == 0) {
        dx_sb_init(&sb, buf, sizeof(buf));
        dx_sb_str(&sb, "No map");
    } else {
        dx_sb_init(&sb, buf, sizeof(buf));
        dx_sb_str(&sb, m->maps[0].title);
        dx_sb_str(&sb, ", ");
        dx_sb_uint(&sb, (uint32_t)m->maps[0].place_count);
        dx_sb_str(&sb, m->maps[0].place_count == 1 ? " place" : " places");
    }
    dx_placeholder_lines(ctx, area, buf, NULL);
}

const dx_screen_t dx_screen_map = {
    "MAP", NULL, NULL, NULL, NULL, draw, NULL, NULL,
};
