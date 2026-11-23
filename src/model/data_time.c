/* Dates and times for the DATA tab's clock. See data_time.h. */
#include "model/data_time.h"

#define SECS_DAY 86400u

int dx_dt_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int dx_dt_month_days(int year, int month)
{
    static const int len[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month < 1 || month > 12) {
        return 0;
    }
    return month == 2 && dx_dt_leap(year) ? 29 : len[month - 1];
}

/* Days from 1970-01-01 to the date: the civil-from-days inverse, with
 * March as the first month so the leap day ends the year. */
static uint32_t days_from_civil(int y, int m, int d)
{
    uint32_t era, yoe, doy, doe;

    y -= m <= 2;
    era = (uint32_t)(y / 400);
    yoe = (uint32_t)(y - (int)era * 400);
    doy = (uint32_t)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097u + doe - 719468u;
}

int dx_dt_weekday(int year, int month, int day)
{
    /* 1970-01-01 was a Thursday: 3 from Monday. */
    return (int)((days_from_civil(year, month, day) + 3u) % 7u);
}

const char *dx_dt_weekday_name(int wd)
{
    static const char *const names[7] = { "Monday", "Tuesday", "Wednesday", "Thursday",
                                          "Friday", "Saturday", "Sunday" };

    return wd >= 0 && wd < 7 ? names[wd] : "";
}

uint32_t dx_dt_to_unix(const dx_dt_t *t)
{
    return days_from_civil(t->year, t->month, t->day) * SECS_DAY +
           (uint32_t)(t->hour * 3600 + t->minute * 60 + t->second);
}

void dx_dt_from_unix(uint32_t s, dx_dt_t *t)
{
    uint32_t z = s / SECS_DAY + 719468u;
    uint32_t rem = s % SECS_DAY;
    uint32_t era = z / 146097u;
    uint32_t doe = z - era * 146097u;
    uint32_t yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    uint32_t doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    uint32_t mp = (5u * doy + 2u) / 153u;
    int m = (int)(mp < 10u ? mp + 3u : mp - 9u);

    t->year = (int)(yoe + era * 400u) + (m <= 2);
    t->month = m;
    t->day = (int)(doy - (153u * mp + 2u) / 5u + 1u);
    t->hour = (int)(rem / 3600u);
    t->minute = (int)(rem / 60u % 60u);
    t->second = (int)(rem % 60u);
}

static int wrap(int v, int lo, int hi)
{
    if (v < lo) {
        return hi;
    }
    if (v > hi) {
        return lo;
    }
    return v;
}

void dx_dt_step(dx_dt_t *t, int field, int delta)
{
    int len;

    delta = delta < 0 ? -1 : 1;
    switch (field) {
    case DX_DT_YEAR:
        t->year = wrap(t->year + delta, DX_DT_YEAR_MIN, DX_DT_YEAR_MAX);
        break;
    case DX_DT_MONTH:
        t->month = wrap(t->month + delta, 1, 12);
        break;
    case DX_DT_DAY:
        t->day = wrap(t->day + delta, 1, dx_dt_month_days(t->year, t->month));
        break;
    case DX_DT_HOUR:
        t->hour = wrap(t->hour + delta, 0, 23);
        break;
    case DX_DT_MINUTE:
        t->minute = wrap(t->minute + delta, 0, 59);
        break;
    default:
        return;
    }
    len = dx_dt_month_days(t->year, t->month);
    if (t->day > len) {
        t->day = len;
    }
}

static void two(char *p, int v)
{
    p[0] = (char)('0' + v / 10 % 10);
    p[1] = (char)('0' + v % 10);
}

void dx_dt_time_text(int valid, const dx_dt_t *t, char out[9])
{
    if (!valid) {
        out[0] = out[1] = out[3] = out[4] = out[6] = out[7] = '-';
    } else {
        two(out, t->hour);
        two(out + 3, t->minute);
        two(out + 6, t->second);
    }
    out[2] = out[5] = ':';
    out[8] = '\0';
}

void dx_dt_date_text(const dx_dt_t *t, char out[11])
{
    two(out, t->year / 100);
    two(out + 2, t->year % 100);
    out[4] = out[7] = '-';
    two(out + 5, t->month);
    two(out + 8, t->day);
    out[10] = '\0';
}
