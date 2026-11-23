#include "ui/kv.h"

#define PAD 4

void dx_kv_init(dx_kv_t *kv, const dx_kv_row_t *rows, int count)
{
    int i;

    kv->rows = rows;
    kv->count = count < 0 ? 0 : count;
    kv->key_cols = 0;
    for (i = 0; i < kv->count; i++) {
        int n = dx_strlen(rows[i].key);

        if (n > kv->key_cols) {
            kv->key_cols = n;
        }
    }
}

void dx_kv_draw(const dx_kv_t *kv, const dx_ctx_t *ctx, dx_rect_t area)
{
    int cols = (area.w - 2 * PAD) / DX_FONT_W;
    int key_cols = kv->key_cols < cols / 2 ? kv->key_cols : cols / 2;
    int vx = area.x + PAD + (key_cols + 2) * DX_FONT_W;
    int i;

    for (i = 0; i < kv->count && (i + 1) * DX_ROW_H <= area.h; i++) {
        int y = area.y + i * DX_ROW_H;

        dx_text_cut(ctx, area.x + PAD, y, kv->rows[i].key, key_cols, ctx->theme.dim,
                    DX_COLOR_NONE);
        dx_text_cut(ctx, vx, y, kv->rows[i].value, cols - key_cols - 2, ctx->theme.fg,
                    DX_COLOR_NONE);
    }
}
