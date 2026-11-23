/*
 * Dates and times for the DATA tab's clock: leap years, month lengths,
 * the weekday, Unix seconds to fields and back, and the setter's field
 * steps. Pure code. The range is 2000 to 2099, what a clock chip
 * stores. Times are UTC.
 */
#ifndef DX_MODEL_DATA_TIME_H
#define DX_MODEL_DATA_TIME_H

#include <stdint.h>

#define DX_DT_YEAR_MIN 2000
#define DX_DT_YEAR_MAX 2099

/* The setter's fields, in order. */
enum { DX_DT_YEAR, DX_DT_MONTH, DX_DT_DAY, DX_DT_HOUR, DX_DT_MINUTE, DX_DT_FIELDS };

typedef struct dx_dt {
    int year;   /* 2000 to 2099 */
    int month;  /* 1 to 12 */
    int day;    /* 1 to the month's length */
    int hour;   /* 0 to 23 */
    int minute; /* 0 to 59 */
    int second; /* 0 to 59 */
} dx_dt_t;

/* 1 for a leap year of the Gregorian calendar. */
int dx_dt_leap(int year);

/* The days in a month, 28 to 31, or 0 for a month outside 1 to 12. */
int dx_dt_month_days(int year, int month);

/* The weekday: 0 Monday to 6 Sunday. */
int dx_dt_weekday(int year, int month, int day);

/* "Monday" to "Sunday" for 0 to 6, else "". */
const char *dx_dt_weekday_name(int wd);

/* The Unix seconds of a valid date and time. */
uint32_t dx_dt_to_unix(const dx_dt_t *t);

/* The fields of Unix seconds. Before 2000 or after 2099 the year is
 * outside the range but still right. */
void dx_dt_from_unix(uint32_t s, dx_dt_t *t);

/*
 * Steps one field by +1 or -1. A field wraps at its ends: year 2000 to
 * 2099, month 1 to 12, day 1 to the month's length, hour 0 to 23,
 * minute 0 to 59. A change of year or month clamps the day to the new
 * month's length.
 */
void dx_dt_step(dx_dt_t *t, int field, int delta);

/* "HH:MM:SS" into out[9]; "--:--:--" when !valid. */
void dx_dt_time_text(int valid, const dx_dt_t *t, char out[9]);

/* "YYYY-MM-DD" into out[11]. */
void dx_dt_date_text(const dx_dt_t *t, char out[11]);

#endif
