/*
 * The uptime service: Pip-OS's ticks_ms wraps at 2^31 ms, 24.8 days, and
 * is the only uptime source. The service samples it at least every
 * DX_UPTIME_SAMPLE_MS and counts the wraps, so the uptime goes on past
 * the wrap. Any caller of dx_uptime_s() samples too.
 */
#include "services/services.h"

static uint32_t last;  /* the last sample */
static uint32_t wraps; /* the wraps seen since the start */

static void sample(uint32_t now_ms)
{
    now_ms &= DX_TICKS_MASK;
    /* A wrap takes the counter from near 2^31 to near 0. A sample at
     * least every DX_UPTIME_SAMPLE_MS sees it as a fall of more than
     * 2^30; a smaller fall is a clock read out of order, not a wrap. */
    if (now_ms < last && last - now_ms > (DX_TICKS_MASK >> 1)) {
        wraps++;
    }
    last = now_ms;
}

/* 2^31 ms is 2147483 s and 648 ms. No 64-bit division: the ROM has no
 * libgcc. */
#define WRAP_S  2147483u
#define WRAP_MS 648u

uint32_t dx_uptime_s(uint32_t now_ms, uint32_t *ms_in_s)
{
    uint32_t rest;

    sample(now_ms);
    rest = (wraps % 1000u) * WRAP_MS + last; /* below 2^31 + 648000 */
    if (ms_in_s != NULL) {
        *ms_in_s = rest % 1000u;
    }
    return wraps * WRAP_S + (wraps / 1000u) * WRAP_MS + rest / 1000u;
}

void dx_uptime_reset(uint32_t now_ms)
{
    last = now_ms & DX_TICKS_MASK;
    wraps = 0;
}

static void init(uint32_t now_ms)
{
    sample(now_ms);
}

static uint32_t tick(uint32_t now_ms, unsigned *flags)
{
    (void)flags;
    sample(now_ms);
    return dx_ticks_add(now_ms, DX_UPTIME_SAMPLE_MS);
}

const dx_service_t dx_service_uptime = { "uptime", init, tick, NULL };
