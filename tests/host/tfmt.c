#include "tfmt.h"

#include <stdio.h>
#include <stdlib.h>

int tvfmt(char *buf, size_t size, const char *fmt, va_list ap)
{
    int n = vsnprintf(buf, size, fmt, ap);

    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr, "tfmt: \"%s\" needs %d bytes, buffer has %zu\n", fmt, n + 1, size);
        abort();
    }
    return n;
}

int tfmt(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = tvfmt(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
