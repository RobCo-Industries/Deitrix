/* Tab moves with wrap-around. */
#include "rc_test.h"

#include "model/nav.h"

/* CHECK_EQ compares as unsigned 64-bit, so compare ints through this. */
#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

TEST(next_and_previous)
{
    CHECK_INT(dx_nav_move(0, 1, 5), 1);
    CHECK_INT(dx_nav_move(3, 1, 5), 4);
    CHECK_INT(dx_nav_move(1, -1, 5), 0);
    CHECK_INT(dx_nav_move(2, 0, 5), 2);
}

TEST(wrap)
{
    CHECK_INT(dx_nav_move(4, 1, 5), 0);
    CHECK_INT(dx_nav_move(0, -1, 5), 4);
    CHECK_INT(dx_nav_move(6, 1, 7), 0);
    CHECK_INT(dx_nav_move(0, -1, 7), 6);
}

TEST(several_detents)
{
    CHECK_INT(dx_nav_move(0, 3, 5), 3);
    CHECK_INT(dx_nav_move(3, 3, 5), 1);
    CHECK_INT(dx_nav_move(1, -3, 5), 3);
    CHECK_INT(dx_nav_move(2, 5, 5), 2);
    CHECK_INT(dx_nav_move(2, -10, 5), 2);
    CHECK_INT(dx_nav_move(0, 12, 5), 2);
    CHECK_INT(dx_nav_move(0, -12, 5), 3);
}

TEST(extreme_deltas)
{
    /* 2147483647 % 5 == 2, -2147483648 % 5 == -3, so +2. */
    CHECK_INT(dx_nav_move(0, INT32_MAX, 5), 2);
    CHECK_INT(dx_nav_move(0, INT32_MIN, 5), 2);
    CHECK_INT(dx_nav_move(4, INT32_MAX, 5), 1);
}

TEST(no_tabs)
{
    CHECK_INT(dx_nav_move(0, 1, 0), 0);
    CHECK_INT(dx_nav_move(0, 1, 1), 0);
}
