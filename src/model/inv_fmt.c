#include "model/inv_fmt.h"

int dx_inv_cat_entries(const dx_items_t *it, int cat)
{
    if (cat < 0 || cat >= it->cat_count) {
        return 0;
    }
    return dx_items_total(it, cat).entries;
}

int dx_inv_item_at(const dx_items_t *it, int cat, int row)
{
    int i;

    if (cat < 0 || cat >= it->cat_count || row < 0) {
        return -1;
    }
    for (i = 0; i < it->item_count; i++) {
        if (it->items[i].cat == cat) {
            if (row == 0) {
                return i;
            }
            row--;
        }
    }
    return -1;
}

void dx_inv_fmt_count(dx_sb_t *sb, const dx_item_t *item)
{
    dx_sb_uint(sb, item->count);
    if (item->unit[0] != '\0') {
        dx_sb_char(sb, ' ');
        dx_sb_str(sb, item->unit);
    }
}

void dx_inv_fmt_weight(dx_sb_t *sb, uint32_t grams)
{
    uint32_t tenths;

    if (grams < 1000) {
        dx_sb_uint(sb, grams);
        dx_sb_str(sb, " g");
        return;
    }
    /* No overflow: 64 items of 999999 g stay far below 2^32. */
    tenths = (grams + 50) / 100;
    dx_sb_uint(sb, tenths / 10);
    dx_sb_char(sb, '.');
    dx_sb_char(sb, (char)('0' + tenths % 10));
    dx_sb_str(sb, " kg");
}

void dx_inv_fmt_summary(dx_sb_t *sb, const dx_items_t *it, int cat)
{
    dx_cat_total_t t = dx_items_total(it, cat);

    dx_sb_uint(sb, (uint32_t)t.entries);
    dx_sb_str(sb, t.entries == 1 ? " entry" : " entries");
    if (t.weighed) {
        dx_sb_str(sb, ", ");
        dx_inv_fmt_weight(sb, t.grams);
    }
}
