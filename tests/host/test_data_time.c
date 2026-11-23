/* The DATA clock's date helpers: leap years, month lengths, weekdays,
 * the Unix conversions against known dates and a day-by-day reference,
 * the setter's steps with wraps and clamps, and the texts. */
#include "rc_test.h"

#include "model/data_time.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static dx_dt_t dt(int y, int mo, int d, int h, int mi, int s)
{
    dx_dt_t t = { y, mo, d, h, mi, s };

    return t;
}

TEST(leap_years)
{
    CHECK_INT(dx_dt_leap(2000), 1);
    CHECK_INT(dx_dt_leap(2024), 1);
    CHECK_INT(dx_dt_leap(2023), 0);
    CHECK_INT(dx_dt_leap(2100), 0);
    CHECK_INT(dx_dt_leap(1900), 0);
    CHECK_INT(dx_dt_month_days(2024, 2), 29);
    CHECK_INT(dx_dt_month_days(2023, 2), 28);
    CHECK_INT(dx_dt_month_days(2023, 4), 30);
    CHECK_INT(dx_dt_month_days(2023, 12), 31);
    CHECK_INT(dx_dt_month_days(2023, 0), 0);
    CHECK_INT(dx_dt_month_days(2023, 13), 0);
}

/* Known dates: the Unix seconds and the weekday. */
TEST(known_dates)
{
    static const struct {
        dx_dt_t t;
        uint32_t unix_s;
        int wd;
    } table[] = {
        { { 1970, 1, 1, 0, 0, 0 }, 0u, 3 },
        { { 2000, 1, 1, 0, 0, 0 }, 946684800u, 5 },
        { { 2024, 2, 29, 12, 0, 0 }, 1709208000u, 3 },
        { { 2038, 1, 19, 3, 14, 7 }, 2147483647u, 1 },
        { { 2099, 12, 31, 23, 59, 59 }, 4102444799u, 3 },
        { { 2026, 11, 23, 0, 0, 0 }, 1795392000u, 0 },
    };
    dx_dt_t got;
    size_t i;

    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        CHECK_INT(dx_dt_to_unix(&table[i].t), table[i].unix_s);
        dx_dt_from_unix(table[i].unix_s, &got);
        CHECK_INT(got.year, table[i].t.year);
        CHECK_INT(got.month, table[i].t.month);
        CHECK_INT(got.day, table[i].t.day);
        CHECK_INT(got.hour, table[i].t.hour);
        CHECK_INT(got.minute, table[i].t.minute);
        CHECK_INT(got.second, table[i].t.second);
        CHECK_INT(dx_dt_weekday(table[i].t.year, table[i].t.month, table[i].t.day),
                  table[i].wd);
    }
    CHECK_STR(dx_dt_weekday_name(0), "Monday");
    CHECK_STR(dx_dt_weekday_name(6), "Sunday");
    CHECK_STR(dx_dt_weekday_name(7), "");
    CHECK_STR(dx_dt_weekday_name(-1), "");
}

/* Every day from 2000 to 2099 against a plain counting loop. */
TEST(every_day_against_a_reference)
{
    uint32_t days = 10957u; /* 1970-01-01 to 2000-01-01 */
    int wd = 5;             /* 2000-01-01 was a Saturday */
    dx_dt_t t, got;
    int y, m, d, bad = 0;

    for (y = DX_DT_YEAR_MIN; y <= DX_DT_YEAR_MAX; y++) {
        for (m = 1; m <= 12; m++) {
            for (d = 1; d <= dx_dt_month_days(y, m); d++) {
                t = dt(y, m, d, 23, 59, 30);
                if (dx_dt_to_unix(&t) != days * 86400u + 86370u) {
                    bad++;
                }
                dx_dt_from_unix(days * 86400u + 86370u, &got);
                if (got.year != y || got.month != m || got.day != d || got.hour != 23 ||
                    got.minute != 59 || got.second != 30) {
                    bad++;
                }
                if (dx_dt_weekday(y, m, d) != wd) {
                    bad++;
                }
                days++;
                wd = (wd + 1) % 7;
            }
        }
    }
    CHECK_INT(bad, 0);
    CHECK_INT(days, 47482u); /* 2100-01-01 */
}

TEST(step_wraps_each_field)
{
    dx_dt_t t = dt(2099, 12, 31, 23, 59, 0);

    dx_dt_step(&t, DX_DT_YEAR, 1);
    CHECK_INT(t.year, 2000);
    dx_dt_step(&t, DX_DT_YEAR, -1);
    CHECK_INT(t.year, 2099);
    dx_dt_step(&t, DX_DT_MONTH, 1);
    CHECK_INT(t.month, 1);
    dx_dt_step(&t, DX_DT_MONTH, -1);
    CHECK_INT(t.month, 12);
    dx_dt_step(&t, DX_DT_DAY, 1);
    CHECK_INT(t.day, 1);
    dx_dt_step(&t, DX_DT_DAY, -1);
    CHECK_INT(t.day, 31);
    dx_dt_step(&t, DX_DT_HOUR, 1);
    CHECK_INT(t.hour, 0);
    dx_dt_step(&t, DX_DT_HOUR, -1);
    CHECK_INT(t.hour, 23);
    dx_dt_step(&t, DX_DT_MINUTE, 1);
    CHECK_INT(t.minute, 0);
    dx_dt_step(&t, DX_DT_MINUTE, -1);
    CHECK_INT(t.minute, 59);
    t.minute = 30;
    dx_dt_step(&t, DX_DT_MINUTE, 5); /* any positive delta is one step */
    CHECK_INT(t.minute, 31);
    dx_dt_step(&t, DX_DT_FIELDS, 1); /* no such field: nothing */
    CHECK_INT(t.year, 2099);
    CHECK_INT(t.minute, 31);
}

TEST(step_clamps_the_day)
{
    dx_dt_t t = dt(2024, 2, 29, 0, 0, 0);

    /* Feb 29 of a leap year to a year without one. */
    dx_dt_step(&t, DX_DT_YEAR, 1);
    CHECK_INT(t.year, 2025);
    CHECK_INT(t.day, 28);
    /* The day wraps at 28 in a common February and at 29 in a leap one. */
    dx_dt_step(&t, DX_DT_DAY, 1);
    CHECK_INT(t.day, 1);
    t = dt(2024, 2, 28, 0, 0, 0);
    dx_dt_step(&t, DX_DT_DAY, 1);
    CHECK_INT(t.day, 29);
    dx_dt_step(&t, DX_DT_DAY, 1);
    CHECK_INT(t.day, 1);
    dx_dt_step(&t, DX_DT_DAY, -1);
    CHECK_INT(t.day, 29);
    /* Day 31 to a 30-day month, both ways. */
    t = dt(2026, 3, 31, 0, 0, 0);
    dx_dt_step(&t, DX_DT_MONTH, 1);
    CHECK_INT(t.month, 4);
    CHECK_INT(t.day, 30);
    t = dt(2026, 5, 31, 0, 0, 0);
    dx_dt_step(&t, DX_DT_MONTH, -1);
    CHECK_INT(t.day, 30);
    /* January 31 to February. */
    t = dt(2026, 1, 31, 0, 0, 0);
    dx_dt_step(&t, DX_DT_MONTH, 1);
    CHECK_INT(t.day, 28);
    /* A day that fits stays. */
    t = dt(2026, 1, 15, 0, 0, 0);
    dx_dt_step(&t, DX_DT_MONTH, 1);
    CHECK_INT(t.day, 15);
}

TEST(texts)
{
    dx_dt_t t = dt(2026, 11, 3, 7, 5, 9);
    char tm[9], dd[11];

    dx_dt_time_text(1, &t, tm);
    CHECK_STR(tm, "07:05:09");
    dx_dt_time_text(0, &t, tm);
    CHECK_STR(tm, "--:--:--");
    dx_dt_date_text(&t, dd);
    CHECK_STR(dd, "2026-11-03");
    t = dt(2099, 12, 31, 23, 59, 59);
    dx_dt_time_text(1, &t, tm);
    CHECK_STR(tm, "23:59:59");
    dx_dt_date_text(&t, dd);
    CHECK_STR(dd, "2099-12-31");
}
