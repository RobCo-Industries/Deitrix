#include "ui/gauge.h"

#define PAD   4
#define GAP   8
#define BAR_H 10

void dx_gauge_init(dx_gauge_t *g, const char *label, int32_t value, int32_t max)
{
    g->label = label;
    g->value = value;
    g->max = max;
}

int32_t dx_gauge_clamp(int32_t value, int32_t max)
{
    if (max <= 0 || value <= 0) {
        return 0;
    }
    return value > max ? max : value;
}

int dx_gauge_fill(int32_t value, int32_t max, int width)
{
    if (max <= 0 || width <= 0) {
        return 0;
    }
    return (int)((int64_t)dx_gauge_clamp(value, max) * width / max);
}

void dx_gauge_draw(const dx_gauge_t *g, const dx_ctx_t *ctx, dx_rect_t area, int label_cols)
{
    const dx_theme_t *t = &ctx->theme;
    int32_t max = g->max < 0 ? 0 : g->max;
    char nums[26];
    int n = dx_itoa(dx_gauge_clamp(g->value, max), nums);
    int bx = area.x + PAD + label_cols * DX_FONT_W + GAP;
    int nx, bw, fill;

    nums[n++] = '/';
    dx_itoa(max, nums + n);
    nx = area.x + area.w - PAD - dx_text_width(nums, 1);
    bw = nx - GAP - bx;
    dx_text_cut(ctx, area.x + PAD, area.y, g->label, label_cols, t->dim, DX_COLOR_NONE);
    ctx->gfx->text(nx, area.y, nums, t->fg, DX_COLOR_NONE, 1);
    if (bw < 3) {
        return;
    }
    ctx->gfx->rect(bx, area.y + (DX_ROW_H - BAR_H) / 2, bw, BAR_H, t->dim, 0);
    fill = dx_gauge_fill(g->value, max, bw - 4);
    if (fill > 0) {
        ctx->gfx->rect(bx + 2, area.y + (DX_ROW_H - BAR_H) / 2 + 2, fill, BAR_H - 4, t->fg, 1);
    }
}
