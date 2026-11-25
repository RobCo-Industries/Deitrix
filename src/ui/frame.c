#include "ui/frame.h"

#define TAB_X0   4 /* left edge of the first tab */
#define TAB_PAD  8 /* space on each side of a tab label */
#define HEADER_Y 3 /* text row of the header */

dx_rect_t dx_frame_body(void)
{
    dx_rect_t r = { 0, DX_BODY_TOP, DX_SCREEN_W, DX_BODY_H };

    return r;
}

/* The width of a label cut to `cut` letters. */
static int label_width(const char *name, int cut)
{
    int n = dx_strlen(name);

    return (n < cut ? n : cut) * DX_FONT_W;
}

/* The pad for labels cut to `cut` letters: TAB_PAD at most, or less to
 * fit, or 0 when they do not fit at DX_TAB_MIN_PAD. */
static int pad_for(const char *const *names, int count, int cut)
{
    int text = 0, pad, i;

    for (i = 0; i < count; i++) {
        text += label_width(names[i], cut);
    }
    pad = (DX_TAB_RIGHT - TAB_X0 - text) / (2 * count);
    if (pad > TAB_PAD) {
        pad = TAB_PAD;
    }
    return pad < DX_TAB_MIN_PAD ? 0 : pad;
}

int dx_frame_tab_cut(const char *const *names, int count)
{
    int cut, i;

    if (names == NULL || count < 1 || count > DX_MAX_TABS) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        int n = names[i] != NULL ? dx_strlen(names[i]) : 0;

        if (n < 1 || n > DX_TAB_NAME_MAX) {
            return 0;
        }
    }
    for (cut = DX_TAB_NAME_MAX; cut >= DX_TAB_CUT_MIN; cut--) {
        if (pad_for(names, count, cut) > 0) {
            return cut;
        }
    }
    return 0;
}

int dx_frame_tab_pad(const char *const *names, int count)
{
    int cut = dx_frame_tab_cut(names, count);

    return cut > 0 ? pad_for(names, count, cut) : 0;
}

int dx_frame_tab_x(const char *const *names, int count, int tab)
{
    int cut = dx_frame_tab_cut(names, count);
    int pad = dx_frame_tab_pad(names, count);
    int x = TAB_X0;
    int i;

    for (i = 0; i < tab && i < count; i++) {
        x += label_width(names[i], cut) + 2 * pad;
    }
    return x + pad;
}

void dx_frame_header(const dx_ctx_t *ctx, const char *const *names, int count, int active,
                     const char *clock)
{
    const dx_gfx_t *g = ctx->gfx;
    const dx_theme_t *t = &ctx->theme;
    int cut = dx_frame_tab_cut(names, count);
    int pad = dx_frame_tab_pad(names, count);
    int x = TAB_X0 + pad;
    int i;

    /* A table that does not fit draws no tabs at all. */
    for (i = 0; cut > 0 && i < count; i++) {
        char label[DX_TAB_NAME_MAX + 1];
        int n = 0;
        int w;

        while (n < cut && names[i][n] != '\0') {
            label[n] = names[i][n];
            n++;
        }
        label[n] = '\0';
        w = n * DX_FONT_W;
        if (i == active) {
            /* The active tab: dark text on a filled block. */
            g->rect(x - pad, 0, w + 2 * pad, DX_HEADER_RULE, t->fg, 1);
            g->text(x, HEADER_Y, label, t->bg, DX_COLOR_NONE, 1);
        } else {
            g->text(x, HEADER_Y, label, t->dim, DX_COLOR_NONE, 1);
        }
        x += w + 2 * pad;
    }
    g->text(DX_CLOCK_X, HEADER_Y, clock, t->fg, DX_COLOR_NONE, 1);
    g->line(0, DX_HEADER_RULE, DX_SCREEN_W - 1, DX_HEADER_RULE, t->fg);
}

void dx_frame_mark(const dx_ctx_t *ctx)
{
    ctx->gfx->text(DX_MARK_X, HEADER_Y, "!", ctx->theme.fg, DX_COLOR_NONE, 1);
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
