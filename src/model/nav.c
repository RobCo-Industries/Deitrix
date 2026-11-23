#include "model/nav.h"

int dx_nav_move(int tab, int32_t delta, int count)
{
    int64_t next;

    if (count <= 0) {
        return 0;
    }
    /* 64-bit, so INT32_MIN and a large tab cannot overflow. */
    next = ((int64_t)tab + delta) % count;
    if (next < 0) {
        next += count;
    }
    return (int)next;
}
