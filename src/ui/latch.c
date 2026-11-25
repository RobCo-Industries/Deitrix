#include "ui/latch.h"

void dx_latch_init(dx_latch_t *l)
{
    l->on = 0;
    l->key = 0;
}

int dx_latch_swallow(dx_latch_t *l, const dx_ui_event_t *ev)
{
    if (!ev->repeat) {
        l->on = 0;
        return 0;
    }
    return l->on && ev->type == l->key;
}

void dx_latch_note(dx_latch_t *l, const dx_ui_event_t *ev, unsigned result, int tab_changed)
{
    if (ev->repeat || ev->type == DX_UI_DIAL) {
        return;
    }
    if ((result & DX_UI_LEVEL) || tab_changed) {
        l->on = 1;
        l->key = ev->type;
    }
}
