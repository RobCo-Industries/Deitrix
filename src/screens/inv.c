/* The INV tab: the inventory screen. A placeholder for now, with the item and category counts. */
#include "screens/screens.h"

#include "content.h"

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_items_t *it = dx_content_items();
    char buf[40];
    dx_sb_t sb;

    (void)st;
    dx_placeholder_draw(ctx, area, "INV");
    if (it->load.status != DX_LOAD_OK) {
        dx_placeholder_reason(&it->load, "ITEMS.INI", buf, sizeof(buf));
    } else {
        dx_sb_init(&sb, buf, sizeof(buf));
        dx_sb_uint(&sb, (uint32_t)it->item_count);
        dx_sb_str(&sb, " items in ");
        dx_sb_uint(&sb, (uint32_t)it->cat_count);
        dx_sb_str(&sb, " categories");
    }
    dx_placeholder_lines(ctx, area, buf, NULL);
}

const dx_screen_t dx_screen_inv = {
    "INV", NULL, NULL, NULL, NULL, draw, NULL, NULL,
};
