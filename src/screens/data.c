/* The DATA tab: the notes and data screen. A placeholder for now, with the note count. */
#include "screens/screens.h"

#include "content.h"

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_notes_t *n = dx_content_notes();
    char buf[40];
    dx_sb_t sb;

    (void)st;
    dx_placeholder_draw(ctx, area, "DATA");
    if (n->load.status != DX_LOAD_OK) {
        dx_placeholder_reason(&n->load, "NOTES", buf, sizeof(buf));
    } else {
        dx_sb_init(&sb, buf, sizeof(buf));
        dx_sb_uint(&sb, (uint32_t)n->count);
        dx_sb_str(&sb, n->count == 1 ? " note" : " notes");
    }
    dx_placeholder_lines(ctx, area, buf, NULL);
}

const dx_screen_t dx_screen_data = {
    "DATA", NULL, NULL, NULL, NULL, draw, NULL, NULL,
};
