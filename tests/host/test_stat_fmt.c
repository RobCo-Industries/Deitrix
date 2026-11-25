/* The STAT tab's text helpers: versions, the SoC, memory, uptime and date. */
#include "rc_test.h"

#include "model/stat_fmt.h"

static const char *up(uint32_t ms)
{
    static char buf[DX_STAT_TEXT_SIZE];

    dx_stat_uptime(ms / 1000u, buf);
    return buf;
}

static const char *date(int valid, uint32_t s)
{
    static char buf[DX_STAT_TEXT_SIZE];

    dx_stat_datetime(valid, s, buf);
    return buf;
}

TEST(uptime_text)
{
    CHECK_STR(up(0), "0d 00:00:00");
    CHECK_STR(up(999), "0d 00:00:00");
    CHECK_STR(up(1000), "0d 00:00:01");
    CHECK_STR(up(3599999), "0d 00:59:59");
    CHECK_STR(up(86399999), "0d 23:59:59");
    CHECK_STR(up(86400000), "1d 00:00:00");
    CHECK_STR(up(90061000), "1d 01:01:01");
    /* The last tick before the 2^31 wrap, and the largest value. */
    CHECK_STR(up(0x7fffffffu), "24d 20:31:23");
    CHECK_STR(up(0xffffffffu), "49d 17:02:47");
}

TEST(date_text)
{
    CHECK_STR(date(0, 12345), "no clock");
    CHECK_STR(date(1, 0), "1970-01-01 00:00:00 UTC");
    CHECK_STR(date(1, 3402208025u), "2077-10-23 09:47:05 UTC");
    CHECK_STR(date(1, 951782400u), "2000-02-29 00:00:00 UTC");
    CHECK_STR(date(1, 951868799u), "2000-02-29 23:59:59 UTC");
    CHECK_STR(date(1, 951868800u), "2000-03-01 00:00:00 UTC");
    /* 2100 is not a leap year. */
    CHECK_STR(date(1, 4107542400u), "2100-03-01 00:00:00 UTC");
    CHECK_STR(date(1, 4107542399u), "2100-02-28 23:59:59 UTC");
    CHECK_STR(date(1, 1704067199u), "2023-12-31 23:59:59 UTC");
    CHECK_STR(date(1, 0xffffffffu), "2106-02-07 06:28:15 UTC");
}

TEST(device_text)
{
    char b[DX_STAT_TEXT_SIZE];

    CHECK_STR(dx_stat_rom_version(), "0.1.0");
    dx_stat_version3(0x00070100u, b);
    CHECK_STR(b, "7.1.0");
    dx_stat_version3(0xffffffffu, b);
    CHECK_STR(b, "65535.255.255");
    dx_stat_version2(0x00010002u, b);
    CHECK_STR(b, "1.2");
    dx_stat_version2(0xffffffffu, b);
    CHECK_STR(b, "65535.65535");
    dx_stat_soc(0x2835u, b);
    CHECK_STR(b, "BCM2835");
    dx_stat_soc(0x2837u, b);
    CHECK_STR(b, "BCM2837");
    dx_stat_soc(0x2711u, b);
    CHECK_STR(b, "0x2711");
    dx_stat_soc(0xffffffffu, b);
    CHECK_STR(b, "0xffffffff");
    dx_stat_soc(0, b);
    CHECK_STR(b, "unknown");
    dx_stat_mem(1006632960u, b);
    CHECK_STR(b, "960 MiB");
    dx_stat_mem(1536u, b);
    CHECK_STR(b, "1 KiB");
    dx_stat_mem(1048577u, b);
    CHECK_STR(b, "1024 KiB");
    dx_stat_mem(512u, b);
    CHECK_STR(b, "512 B");
    dx_stat_mem(0xffffffffu, b);
    CHECK_STR(b, "4194303 KiB");
}
