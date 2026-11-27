#include "ui/subtabs.h"

#define PAD  4
#define GAP  16
#define TEXT_Y 1

void dx_subtabs_init(dx_subtabs_t *st, const char *const *labels, int count)
{
    int i;

    if (count > DX_SUBTABS_MAX) {
        count = DX_SUBTABS_MAX;
    }
    st->count = count < 0 ? 0 : count;
    for (i = 0; i < st->count; i++) {
        st->labels[i] = labels[i];
    }
    st->active = 0;
}

unsigned dx_subtabs_event(dx_subtabs_t *st, const dx_ui_event_t *ev)
{
    if (ev->type == DX_UI_LEFT && st->active > 0) {
        st->active--;
    } else if (ev->type == DX_UI_RIGHT && st->active < st->count - 1) {
        st->active++;
    } else {
        return DX_UI_IGNORED;
    }
    return DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_TAB_MOVED;
}

void dx_subtabs_draw(const dx_subtabs_t *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_theme_t *t = &ctx->theme;
    int x = area.x + PAD;
    int i;

    for (i = 0; i < st->count; i++) {
        int w = dx_text_width(st->labels[i], 1);

        if (i == st->active) {
            ctx->gfx->text(x, area.y + TEXT_Y, st->labels[i], t->fg, DX_COLOR_NONE, 1);
            ctx->gfx->rect(x, area.y + TEXT_Y + DX_FONT_H, w, 2, t->fg, 1);
        } else {
            ctx->gfx->text(x, area.y + TEXT_Y, st->labels[i], t->dim, DX_COLOR_NONE, 1);
        }
        x += w + GAP;
    }
    ctx->gfx->line(area.x, area.y + DX_SUBTABS_H - 1, area.x + area.w - 1,
                   area.y + DX_SUBTABS_H - 1, t->faint);
}
