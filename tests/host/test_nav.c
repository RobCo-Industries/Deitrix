/* The tab list and the navigation state machine. */
#include "rc_test.h"

#include "model/nav.h"

/* CHECK_EQ compares as unsigned 64-bit, so compare ints through this. */
#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

TEST(tab_names)
{
    CHECK_STR(dx_tab_name(DX_TAB_STAT), "STAT");
    CHECK_STR(dx_tab_name(DX_TAB_INV), "INV");
    CHECK_STR(dx_tab_name(DX_TAB_DATA), "DATA");
    CHECK_STR(dx_tab_name(DX_TAB_MAP), "MAP");
    CHECK_STR(dx_tab_name(DX_TAB_RADIO), "RADIO");
    CHECK_STR(dx_tab_name(-1), "");
    CHECK_STR(dx_tab_name(DX_TAB_COUNT), "");
}

TEST(next_and_previous)
{
    CHECK_INT(dx_nav_move(DX_TAB_STAT, 1), DX_TAB_INV);
    CHECK_INT(dx_nav_move(DX_TAB_MAP, 1), DX_TAB_RADIO);
    CHECK_INT(dx_nav_move(DX_TAB_INV, -1), DX_TAB_STAT);
    CHECK_INT(dx_nav_move(DX_TAB_DATA, 0), DX_TAB_DATA);
}

TEST(wrap)
{
    CHECK_INT(dx_nav_move(DX_TAB_RADIO, 1), DX_TAB_STAT);
    CHECK_INT(dx_nav_move(DX_TAB_STAT, -1), DX_TAB_RADIO);
}

TEST(several_detents)
{
    CHECK_INT(dx_nav_move(DX_TAB_STAT, 3), DX_TAB_MAP);
    CHECK_INT(dx_nav_move(DX_TAB_MAP, 3), DX_TAB_INV);
    CHECK_INT(dx_nav_move(DX_TAB_INV, -3), DX_TAB_MAP);
    CHECK_INT(dx_nav_move(DX_TAB_DATA, 5), DX_TAB_DATA);
    CHECK_INT(dx_nav_move(DX_TAB_DATA, -10), DX_TAB_DATA);
    CHECK_INT(dx_nav_move(DX_TAB_STAT, 12), DX_TAB_DATA);
    CHECK_INT(dx_nav_move(DX_TAB_STAT, -12), DX_TAB_MAP);
}

TEST(extreme_deltas)
{
    /* 2147483647 % 5 == 2, -2147483648 % 5 == -3, so +2. */
    CHECK_INT(dx_nav_move(DX_TAB_STAT, INT32_MAX), DX_TAB_DATA);
    CHECK_INT(dx_nav_move(DX_TAB_STAT, INT32_MIN), DX_TAB_DATA);
    CHECK_INT(dx_nav_move(DX_TAB_RADIO, INT32_MAX), DX_TAB_INV);
}

static int32_t delta(uint32_t type, uint32_t code, int32_t value)
{
    dx_event_t ev = { type, code, value };

    return dx_nav_delta(&ev);
}

TEST(event_deltas)
{
    CHECK_INT(delta(DX_EV_DIAL, 0, 1), 1);
    CHECK_INT(delta(DX_EV_DIAL, 0, -4), -4);
    CHECK_INT(delta(DX_EV_BUTTON, DX_BTN_LEFT, DX_PRESS_DOWN), -1);
    CHECK_INT(delta(DX_EV_BUTTON, DX_BTN_RIGHT, DX_PRESS_DOWN), 1);
    CHECK_INT(delta(DX_EV_BUTTON, DX_BTN_RIGHT, DX_PRESS_REPEAT), 1);
    CHECK_INT(delta(DX_EV_BUTTON, DX_BTN_LEFT, DX_PRESS_RELEASE), 0);
    CHECK_INT(delta(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_DOWN), 0);
    CHECK_INT(delta(DX_EV_BUTTON, DX_BTN_SELECT, DX_PRESS_DOWN), 0);
    CHECK_INT(delta(DX_EV_BUTTON, DX_BTN_DIAL, DX_PRESS_DOWN), 0);
    CHECK_INT(delta(7, DX_BTN_LEFT, DX_PRESS_DOWN), 0);
}
