#include "model/clock.h"

int32_t dx_clock_minute(int valid, uint32_t unix_seconds)
{
    if (!valid) {
        return DX_CLOCK_NONE;
    }
    return (int32_t)((unix_seconds / 60u) % 1440u);
}

void dx_clock_text(int32_t minute, char out[DX_CLOCK_TEXT_SIZE])
{
    int32_t h, m;

    if (minute < 0 || minute >= 1440) {
        out[0] = '-';
        out[1] = '-';
        out[2] = ':';
        out[3] = '-';
        out[4] = '-';
        out[5] = '\0';
        return;
    }
    h = minute / 60;
    m = minute % 60;
    out[0] = (char)('0' + h / 10);
    out[1] = (char)('0' + h % 10);
    out[2] = ':';
    out[3] = (char)('0' + m / 10);
    out[4] = (char)('0' + m % 10);
    out[5] = '\0';
}

uint32_t dx_clock_wait_ms(uint32_t unix_seconds)
{
    return (60u - unix_seconds % 60u) * 1000u;
}
