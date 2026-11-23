#include "model/profile.h"

/* The keys as they come: slot N - 1, last one wins. */
typedef struct staging {
    dx_profile_t *p;
    uint8_t gauge_on[DX_GAUGES_MAX];
    dx_gauge_t gauges[DX_GAUGES_MAX];
    uint8_t attr_on[DX_ATTRS_MAX];
    dx_attr_t attrs[DX_ATTRS_MAX];
    uint8_t about_on[DX_ABOUT_MAX];
    char about[DX_ABOUT_MAX][DX_ABOUT_LINE_MAX + 1];
} staging_t;

/* A name field: spaces dropped, cut to max. 0 when empty. */
static int take_name(char *dst, int max, const char *s, int n)
{
    int a;

    n = dx_strip(s, n, &a);
    if (n == 0) {
        return 0;
    }
    dx_str_cut(dst, max + 1, s + a, n);
    return 1;
}

static int gauge(dx_gauge_t *g, const char *v)
{
    int st[3], ln[3];
    uint32_t value, max;

    if (dx_split(v, 3, st, ln) != 3) {
        return 0;
    }
    if (!dx_parse_num(v + st[1], ln[1], &max) || max < 1 || max > DX_GAUGE_VALUE_MAX) {
        return 0;
    }
    if (!dx_parse_num(v + st[0], ln[0], &value)) {
        return 0;
    }
    g->max = (uint16_t)max;
    g->value = (uint16_t)(value > max ? max : value);
    return take_name(g->name, DX_GAUGE_NAME_MAX, v + st[2], ln[2]);
}

static int attr(dx_attr_t *a, const char *v)
{
    int st[2], ln[2];
    uint32_t value;

    if (dx_split(v, 2, st, ln) != 2 || !dx_parse_num(v + st[0], ln[0], &value)) {
        return 0;
    }
    a->value = (uint8_t)(value > DX_ATTR_VALUE_MAX ? DX_ATTR_VALUE_MAX : value);
    return take_name(a->name, DX_ATTR_NAME_MAX, v + st[1], ln[1]);
}

static int on_key(const char *section, const char *key, const char *value, void *ctx)
{
    staging_t *s = ctx;
    dx_profile_t *p = s->p;
    int n;

    if (dx_str_eq(section, "owner")) {
        if (dx_str_eq(key, "name")) {
            dx_str_cut(p->name, sizeof(p->name), value, dx_str_len(value));
        } else if (dx_str_eq(key, "line")) {
            dx_str_cut(p->line, sizeof(p->line), value, dx_str_len(value));
        }
    } else if (dx_str_eq(section, "gauges")) {
        n = dx_numbered_key(key, 'g', DX_GAUGES_MAX);
        if (n > 0) {
            s->gauge_on[n - 1] = (uint8_t)gauge(&s->gauges[n - 1], value);
        }
    } else if (dx_str_eq(section, "attrs")) {
        n = dx_numbered_key(key, 'a', DX_ATTRS_MAX);
        if (n > 0) {
            s->attr_on[n - 1] = (uint8_t)attr(&s->attrs[n - 1], value);
        }
    } else if (dx_str_eq(section, "about")) {
        n = dx_numbered_key(key, 'l', DX_ABOUT_MAX);
        if (n > 0) {
            s->about_on[n - 1] = 1;
            dx_str_cut(s->about[n - 1], DX_ABOUT_LINE_MAX + 1, value, dx_str_len(value));
        }
    }
    return 0;
}

void dx_profile_load(dx_profile_t *p, const uint8_t *data, int32_t size)
{
    static staging_t s;
    dx_load_t ld;
    int i;

    dx_mem_set(p, 0, sizeof(*p));
    dx_mem_set(&s, 0, sizeof(s));
    s.p = p;
    if (!dx_load_ini(&ld, data, size, on_key, &s)) {
        dx_mem_set(p, 0, sizeof(*p));
        p->load = ld;
        return;
    }
    p->load = ld;
    for (i = 0; i < DX_GAUGES_MAX; i++) {
        if (s.gauge_on[i]) {
            p->gauges[p->gauge_count++] = s.gauges[i];
        }
    }
    for (i = 0; i < DX_ATTRS_MAX; i++) {
        if (s.attr_on[i]) {
            p->attrs[p->attr_count++] = s.attrs[i];
        }
    }
    /* An empty about line stays: it is a blank row. */
    for (i = 0; i < DX_ABOUT_MAX; i++) {
        if (s.about_on[i]) {
            dx_mem_copy(p->about[p->about_count++], s.about[i], DX_ABOUT_LINE_MAX + 1);
        }
    }
}
