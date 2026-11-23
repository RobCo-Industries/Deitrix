/* The header clock text and the wait to the next minute. */
#include "rc_test.h"

#include "model/clock.h"

static const char *text(int32_t minute)
{
    static char buf[DX_CLOCK_TEXT_SIZE];

    dx_clock_text(minute, buf);
    return buf;
}

TEST(text_bounds)
{
    CHECK_STR(text(0), "00:00");
    CHECK_STR(text(1439), "23:59");
    CHECK_STR(text(9 * 60 + 47), "09:47");
    CHECK_STR(text(12 * 60), "12:00");
}

TEST(text_no_clock)
{
    CHECK_STR(text(DX_CLOCK_NONE), "--:--");
    CHECK_STR(text(1440), "--:--");
    CHECK_STR(text(INT32_MIN), "--:--");
}

TEST(minute_of_day)
{
    CHECK_EQ(dx_clock_minute(1, 0), 0);
    CHECK_EQ(dx_clock_minute(1, 59), 0);
    CHECK_EQ(dx_clock_minute(1, 60), 1);
    CHECK_EQ(dx_clock_minute(1, 86399), 1439);
    CHECK_EQ(dx_clock_minute(1, 86400), 0);
    /* 2077-10-23T09:47:05Z */
    CHECK_EQ(dx_clock_minute(1, 3402208025u), 9 * 60 + 47);
    CHECK_EQ(dx_clock_minute(1, UINT32_MAX), (UINT32_MAX / 60u) % 1440u);
    CHECK_EQ((int64_t)dx_clock_minute(0, 3600), (int64_t)DX_CLOCK_NONE);
}

TEST(wait_to_next_minute)
{
    CHECK_EQ(dx_clock_wait_ms(0), 60000);
    CHECK_EQ(dx_clock_wait_ms(1), 59000);
    CHECK_EQ(dx_clock_wait_ms(59), 1000);
    CHECK_EQ(dx_clock_wait_ms(120), 60000);
}
