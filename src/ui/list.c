#include "ui/list.h"

#define PAD_L   12 /* room for the mark */
#define PAD_R   4
#define GAP     8  /* at least one cell between the label and the value */
#define MARK_X  4
#define MARK_SZ 4

void dx_list_init(dx_list_t *l, int count, int visible, dx_row_fn row, void *user,
                  const char *empty)
{
    l->row = row;
    l->user = user;
    l->empty = empty;
    l->count = count < 0 ? 0 : count;
    l->visible = visible < 1 ? 1 : visible;
    l->sel = 0;
    l->top = 0;
}

int dx_list_window(int sel, int top, int count, int visible)
{
    if (visible < 1 || count <= visible) {
        return 0;
    }
    if (sel < top) {
        top = sel;
    } else if (sel >= top + visible) {
        top = sel - visible + 1;
    }
    if (top > count - visible) {
        top = count - visible;
    }
    return top < 0 ? 0 : top;
}

void dx_list_select(dx_list_t *l, int sel)
{
    if (sel >= l->count) {
        sel = l->count - 1;
    }
    l->sel = sel < 0 ? 0 : sel;
    l->top = dx_list_window(l->sel, l->top, l->count, l->visible);
}

unsigned dx_list_event(dx_list_t *l, const dx_ui_event_t *ev)
{
    int step;

    if (ev->type == DX_UI_UP) {
        step = -1;
    } else if (ev->type == DX_UI_DOWN) {
        step = 1;
    } else {
        return DX_UI_IGNORED;
    }
    if (l->sel + step < 0 || l->sel + step >= l->count) {
        return DX_UI_IGNORED;
    }
    dx_list_select(l, l->sel + step);
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

static void draw_row(const dx_ctx_t *ctx, const dx_row_t *row, int x, int y, int w, int selected)
{
    const dx_theme_t *t = &ctx->theme;
    uint32_t fg = (row->flags & DX_ROW_DISABLED) ? t->dim : t->fg;
    uint32_t bg = DX_COLOR_NONE;
    int lx = x + PAD_L;
    int right = x + w - PAD_R;
    int cols;

    if (selected) {
        ctx->gfx->rect(x, y, w, DX_ROW_H, fg, 1);
        bg = fg;
        fg = t->bg;
    }
    if (row->flags & DX_ROW_MARKED) {
        ctx->gfx->rect(x + MARK_X, y + (DX_ROW_H - MARK_SZ) / 2, MARK_SZ, MARK_SZ, fg, 1);
    }
    if (row->value != NULL && row->value[0] != '\0') {
        int vw = dx_text_width(row->value, 1);

        if (vw > right - lx) {
            /* A value wider than the row takes it all, cut. */
            dx_text_cut(ctx, lx, y, row->value, (right - lx) / DX_FONT_W, fg, bg);
            return;
        }
        ctx->gfx->text(right - vw, y, row->value, fg, bg, 1);
        right -= vw + GAP;
    }
    cols = (right - lx) / DX_FONT_W;
    if (row->label != NULL) {
        dx_text_cut(ctx, lx, y, row->label, cols, fg, bg);
    }
}

void dx_list_draw(const dx_list_t *l, const dx_ctx_t *ctx, dx_rect_t area)
{
    int rows = l->visible;
    int w = area.w;
    int i;

    if (l->count == 0) {
        if (l->empty != NULL) {
            int cols = area.w / DX_FONT_W;
            int n = dx_strlen(l->empty);

            if (n > cols) {
                n = cols;
            }
            dx_text_cut(ctx, area.x + (area.w - n * DX_FONT_W) / 2,
                        area.y + (area.h - DX_ROW_H) / 2, l->empty, cols, ctx->theme.dim,
                        DX_COLOR_NONE);
        }
        return;
    }
    if (rows > area.h / DX_ROW_H) {
        rows = area.h / DX_ROW_H;
    }
    if (l->count > rows) {
        dx_rect_t bar = { area.x + area.w - DX_SCROLLBAR_W, area.y, DX_SCROLLBAR_W,
                          rows * DX_ROW_H };

        w -= DX_SCROLLBAR_W + 2;
        dx_scrollbar(ctx, bar, l->top, rows, l->count);
    }
    for (i = 0; i < rows && l->top + i < l->count; i++) {
        dx_row_t row = { "", NULL, 0 };

        l->row(l->user, l->top + i, &row);
        draw_row(ctx, &row, area.x, area.y + i * DX_ROW_H, w, l->top + i == l->sel);
    }
}
