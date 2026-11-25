#include "model/stat_fmt.h"

#include "model/util.h"

#define STR2(x) #x
#define STR(x)  STR2(x)

const char *dx_stat_rom_version(void)
{
#ifdef DX_VERSION
    return STR(DX_VERSION);
#else
    return "dev";
#endif
}

static void two(dx_sb_t *sb, uint32_t v)
{
    dx_sb_char(sb, (char)('0' + v / 10u % 10u));
    dx_sb_char(sb, (char)('0' + v % 10u));
}

void dx_stat_version3(uint32_t v, char out[DX_STAT_TEXT_SIZE])
{
    dx_sb_t sb;

    dx_sb_init(&sb, out, DX_STAT_TEXT_SIZE);
    dx_sb_uint(&sb, v >> 16);
    dx_sb_char(&sb, '.');
    dx_sb_uint(&sb, (v >> 8) & 0xffu);
    dx_sb_char(&sb, '.');
    dx_sb_uint(&sb, v & 0xffu);
}

void dx_stat_version2(uint32_t v, char out[DX_STAT_TEXT_SIZE])
{
    dx_sb_t sb;

    dx_sb_init(&sb, out, DX_STAT_TEXT_SIZE);
    dx_sb_uint(&sb, v >> 16);
    dx_sb_char(&sb, '.');
    dx_sb_uint(&sb, v & 0xffffu);
}

void dx_stat_soc(uint32_t soc, char out[DX_STAT_TEXT_SIZE])
{
    static const char hex[] = "0123456789abcdef";
    dx_sb_t sb;
    int shift;

    dx_sb_init(&sb, out, DX_STAT_TEXT_SIZE);
    if (soc == 0) {
        dx_sb_str(&sb, "unknown");
        return;
    }
    if (soc >= 0x2835u && soc <= 0x2837u) {
        dx_sb_str(&sb, "BCM");
        dx_sb_char(&sb, '2');
        dx_sb_char(&sb, '8');
        dx_sb_char(&sb, '3');
        dx_sb_char(&sb, (char)('0' + (soc & 0xfu)));
        return;
    }
    dx_sb_str(&sb, "0x");
    for (shift = 28; shift > 0 && (soc >> shift) == 0; shift -= 4) {
    }
    for (; shift >= 0; shift -= 4) {
        dx_sb_char(&sb, hex[(soc >> shift) & 0xfu]);
    }
}

void dx_stat_mem(uint32_t bytes, char out[DX_STAT_TEXT_SIZE])
{
    dx_sb_t sb;

    dx_sb_init(&sb, out, DX_STAT_TEXT_SIZE);
    if (bytes >= 1048576u && bytes % 1048576u == 0) {
        dx_sb_uint(&sb, bytes / 1048576u);
        dx_sb_str(&sb, " MiB");
    } else if (bytes >= 1024u) {
        dx_sb_uint(&sb, bytes / 1024u);
        dx_sb_str(&sb, " KiB");
    } else {
        dx_sb_uint(&sb, bytes);
        dx_sb_str(&sb, " B");
    }
}

static void hms(dx_sb_t *sb, uint32_t s)
{
    two(sb, s / 3600u);
    dx_sb_char(sb, ':');
    two(sb, s / 60u % 60u);
    dx_sb_char(sb, ':');
    two(sb, s % 60u);
}

void dx_stat_uptime(uint32_t seconds, char out[DX_STAT_TEXT_SIZE])
{
    uint32_t s = seconds;
    dx_sb_t sb;

    dx_sb_init(&sb, out, DX_STAT_TEXT_SIZE);
    dx_sb_uint(&sb, s / 86400u);
    dx_sb_str(&sb, "d ");
    hms(&sb, s % 86400u);
}

void dx_stat_datetime(int valid, uint32_t unix_seconds, char out[DX_STAT_TEXT_SIZE])
{
    /* Days to a civil date, from Howard Hinnant's days_from_civil
     * inverse, on unsigned values: 1970 to 2106 fit. */
    uint32_t z, era, doe, yoe, doy, mp, d, m, y;
    dx_sb_t sb;

    dx_sb_init(&sb, out, DX_STAT_TEXT_SIZE);
    if (!valid) {
        dx_sb_str(&sb, "no clock");
        return;
    }
    z = unix_seconds / 86400u + 719468u;
    era = z / 146097u;
    doe = z - era * 146097u;
    yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    y = yoe + era * 400u;
    doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    mp = (5u * doy + 2u) / 153u;
    d = doy - (153u * mp + 2u) / 5u + 1u;
    m = mp < 10u ? mp + 3u : mp - 9u;
    if (m <= 2u) {
        y++;
    }
    dx_sb_uint(&sb, y);
    dx_sb_char(&sb, '-');
    two(&sb, m);
    dx_sb_char(&sb, '-');
    two(&sb, d);
    dx_sb_char(&sb, ' ');
    hms(&sb, unix_seconds % 86400u);
    dx_sb_str(&sb, " UTC");
}
