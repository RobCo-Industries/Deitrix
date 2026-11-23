#include "ui/frame.h"

#define TAB_X0   4 /* left edge of the first tab */
#define TAB_PAD  8 /* space on each side of a tab label */
#define HEADER_Y 3 /* text row of the header */
#define CLOCK_X  (DX_SCREEN_W - 4 - 5 * DX_FONT_W)
#define CLOCK_GAP 8 /* at least this between the last tab and the clock */

dx_rect_t dx_frame_body(void)
{
    dx_rect_t r = { 0, DX_BODY_TOP, DX_SCREEN_W, DX_BODY_H };

    return r;
}

int dx_frame_tab_pad(const char *const *names, int count)
{
    int text = 0, pad, i;

    for (i = 0; i < count; i++) {
        text += dx_text_width(names[i], 1);
    }
    if (count <= 0) {
        return TAB_PAD;
    }
    pad = (CLOCK_X - CLOCK_GAP - TAB_X0 - text) / (2 * count);
    if (pad > TAB_PAD) {
        pad = TAB_PAD;
    }
    return pad < 1 ? 1 : pad;
}

int dx_frame_tab_x(const char *const *names, int count, int tab)
{
    int pad = dx_frame_tab_pad(names, count);
    int x = TAB_X0;
    int i;

    for (i = 0; i < tab && i < count; i++) {
        x += dx_text_width(names[i], 1) + 2 * pad;
    }
    return x + pad;
}

void dx_frame_header(const dx_ctx_t *ctx, const char *const *names, int count, int active,
                     const char *clock)
{
    const dx_gfx_t *g = ctx->gfx;
    const dx_theme_t *t = &ctx->theme;
    int pad = dx_frame_tab_pad(names, count);
    int i;

    for (i = 0; i < count; i++) {
        int x = dx_frame_tab_x(names, count, i);

        if (i == active) {
            /* The active tab: dark text on a filled block. */
            g->rect(x - pad, 0, dx_text_width(names[i], 1) + 2 * pad, DX_HEADER_RULE, t->fg, 1);
            g->text(x, HEADER_Y, names[i], t->bg, DX_COLOR_NONE, 1);
        } else {
            g->text(x, HEADER_Y, names[i], t->dim, DX_COLOR_NONE, 1);
        }
    }
    g->text(CLOCK_X, HEADER_Y, clock, t->fg, DX_COLOR_NONE, 1);
    g->line(0, DX_HEADER_RULE, DX_SCREEN_W - 1, DX_HEADER_RULE, t->fg);
}

void dx_frame_footer(const dx_ctx_t *ctx, const char *hint)
{
    ctx->gfx->line(0, DX_FOOTER_RULE, DX_SCREEN_W - 1, DX_FOOTER_RULE, ctx->theme.fg);
    dx_text_centered(ctx, DX_FOOTER_Y, hint, ctx->theme.dim, 1);
}

void dx_text_centered(const dx_ctx_t *ctx, int y, const char *text, uint32_t color, int scale)
{
    ctx->gfx->text((DX_SCREEN_W - dx_text_width(text, scale)) / 2, y, text, color,
                   DX_COLOR_NONE, scale);
}
