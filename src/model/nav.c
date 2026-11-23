#include "model/nav.h"

static const char *const tab_names[DX_TAB_COUNT] = {
    "STAT", "INV", "DATA", "MAP", "RADIO",
};

const char *dx_tab_name(int tab)
{
    if (tab < 0 || tab >= DX_TAB_COUNT) {
        return "";
    }
    return tab_names[tab];
}

int dx_nav_move(int tab, int32_t delta)
{
    /* 64-bit, so INT32_MIN and a large tab cannot overflow. */
    int64_t next = ((int64_t)tab + delta) % DX_TAB_COUNT;

    if (next < 0) {
        next += DX_TAB_COUNT;
    }
    return (int)next;
}

int32_t dx_nav_delta(const dx_event_t *ev)
{
    if (ev->type == DX_EV_DIAL) {
        return ev->value;
    }
    if (ev->type != DX_EV_BUTTON || ev->value == DX_PRESS_RELEASE) {
        return 0;
    }
    if (ev->code == DX_BTN_LEFT) {
        return -1;
    }
    if (ev->code == DX_BTN_RIGHT) {
        return 1;
    }
    return 0;
}
