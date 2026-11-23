#include "model/util.h"

void dx_sb_init(dx_sb_t *sb, char *buf, int cap)
{
    sb->buf = buf;
    sb->cap = cap;
    sb->len = 0;
    if (cap > 0) {
        buf[0] = '\0';
    }
}

void dx_sb_char(dx_sb_t *sb, char c)
{
    if (sb->len < sb->cap - 1) {
        sb->buf[sb->len++] = c;
        sb->buf[sb->len] = '\0';
    }
}

void dx_sb_str(dx_sb_t *sb, const char *s)
{
    while (*s != '\0') {
        dx_sb_char(sb, *s++);
    }
}

void dx_sb_uint(dx_sb_t *sb, uint32_t v)
{
    char d[10];
    int n = 0;

    do {
        d[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0);
    while (n > 0) {
        dx_sb_char(sb, d[--n]);
    }
}

void dx_sb_int(dx_sb_t *sb, int32_t v)
{
    if (v < 0) {
        dx_sb_char(sb, '-');
        dx_sb_uint(sb, 0u - (uint32_t)v);
    } else {
        dx_sb_uint(sb, (uint32_t)v);
    }
}

int dx_str_len(const char *s)
{
    int n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

int dx_str_cmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int dx_str_eq(const char *a, const char *b)
{
    return dx_str_cmp(a, b) == 0;
}

int dx_str_ends(const char *s, const char *suffix)
{
    int n = dx_str_len(s);
    int m = dx_str_len(suffix);

    return n >= m && dx_str_eq(s + n - m, suffix);
}

void dx_str_cut(char *dst, int cap, const char *src, int n)
{
    int i;

    if (cap <= 0) {
        return;
    }
    if (n > cap - 1) {
        n = cap - 1;
    }
    for (i = 0; i < n; i++) {
        dst[i] = src[i];
    }
    dst[n] = '\0';
}

void dx_mem_set(void *dst, uint8_t v, uint32_t n)
{
    volatile uint8_t *d = dst; /* volatile: no memset call in freestanding code */

    while (n-- > 0) {
        *d++ = v;
    }
}

void dx_mem_copy(void *dst, const void *src, uint32_t n)
{
    volatile uint8_t *d = dst;
    const uint8_t *s = src;

    while (n-- > 0) {
        *d++ = *s++;
    }
}

int dx_strip(const char *s, int n, int *start)
{
    int a = 0;

    while (a < n && s[a] == ' ') {
        a++;
    }
    while (n > a && s[n - 1] == ' ') {
        n--;
    }
    *start = a;
    return n - a;
}

int dx_parse_num(const char *s, int n, uint32_t *out)
{
    int a;
    uint32_t v = 0;
    int i;

    n = dx_strip(s, n, &a);
    if (n < 1 || n > 6) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        char c = s[a + i];

        if (c < '0' || c > '9') {
            return 0;
        }
        v = v * 10u + (uint32_t)(c - '0');
    }
    *out = v;
    return 1;
}

int dx_numbered_key(const char *key, char prefix, int max)
{
    int n;

    if (key[0] != prefix || key[1] < '1' || key[1] > '9') {
        return 0;
    }
    n = key[1] - '0';
    if (key[2] != '\0') {
        if (key[2] < '0' || key[2] > '9' || key[3] != '\0') {
            return 0;
        }
        n = n * 10 + (key[2] - '0');
    }
    return n <= max ? n : 0;
}

int dx_is_id(const char *s)
{
    int n = 0;

    for (; s[n] != '\0'; n++) {
        char c = s[n];

        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') || n >= 12) {
            return 0;
        }
    }
    return n >= 1;
}

static int name83_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

int dx_is_name83(const char *s)
{
    int n = 0;
    int e = 0;

    while (name83_char(s[n])) {
        n++;
    }
    if (n < 1 || n > 8) {
        return 0;
    }
    if (s[n] == '\0') {
        return 1;
    }
    if (s[n] != '.') {
        return 0;
    }
    s += n + 1;
    while (name83_char(s[e])) {
        e++;
    }
    return e >= 1 && e <= 3 && s[e] == '\0';
}

int dx_split(const char *s, int max, int *starts, int *lens)
{
    int count = 0;
    int i = 0;

    starts[0] = 0;
    for (;;) {
        if (s[i] == '\0' || (s[i] == ',' && count < max - 1)) {
            lens[count] = i - starts[count];
            count++;
            if (s[i] == '\0') {
                return count;
            }
            starts[count] = i + 1;
        }
        i++;
    }
}
