#include "model/map.h"

static const char *const kinds[] = { "home", "water", "shelter", "marker", "hazard" };

typedef struct line_stage {
    uint8_t on;
    dx_polyline_t line;
} line_stage_t;

typedef struct place_stage {
    uint8_t on;
    dx_place_t place;
} place_stage_t;

typedef struct map_stage {
    uint8_t head;   /* [map.<name>] has keys */
    uint8_t box_ok;
    char title[DX_MAP_TITLE_MAX + 1];
    int16_t box[4];
    line_stage_t lines[DX_LINES_MAX];
    place_stage_t places[DX_PLACES_MAX];
} map_stage_t;

static map_stage_t stage[DX_MAPS_MAX];

const char *dx_place_kind_name(int kind)
{
    return kind >= 0 && kind < (int)(sizeof(kinds) / sizeof(kinds[0])) ? kinds[kind] : "";
}

/* "x,y" of n bytes, each 0 to 999. */
static int point(const char *s, int n, dx_point_t *p)
{
    int comma = -1;
    int i;
    uint32_t x, y;

    for (i = 0; i < n; i++) {
        if (s[i] == ',') {
            if (comma >= 0) {
                return 0;
            }
            comma = i;
        }
    }
    if (comma < 0 || !dx_parse_num(s, comma, &x) ||
        !dx_parse_num(s + comma + 1, n - comma - 1, &y) || x > DX_COORD_MAX ||
        y > DX_COORD_MAX) {
        return 0;
    }
    p->x = (int16_t)x;
    p->y = (int16_t)y;
    return 1;
}

static int box(const char *v, int16_t out[4])
{
    int st[5], ln[5];
    uint32_t b[4];
    int i;

    if (dx_split(v, 5, st, ln) != 4) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (!dx_parse_num(v + st[i], ln[i], &b[i]) || b[i] > DX_COORD_MAX) {
            return 0;
        }
    }
    if (b[0] >= b[2] || b[1] >= b[3]) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        out[i] = (int16_t)b[i];
    }
    return 1;
}

/* Points separated by spaces or tabs: 2 to 16 of them. */
static int line(const char *v, dx_polyline_t *l)
{
    int i = 0;

    l->count = 0;
    for (;;) {
        int start;

        while (v[i] == ' ' || v[i] == '\t') {
            i++;
        }
        if (v[i] == '\0') {
            break;
        }
        start = i;
        while (v[i] != '\0' && v[i] != ' ' && v[i] != '\t') {
            i++;
        }
        if (l->count >= DX_POINTS_MAX || !point(v + start, i - start, &l->pts[l->count])) {
            return 0;
        }
        l->count++;
    }
    return l->count >= DX_POINTS_MIN;
}

static int place(const char *v, dx_place_t *p)
{
    int st[4], ln[4];
    int a, n;
    int k;
    int x_at, x_len;

    if (dx_split(v, 4, st, ln) != 4) {
        return 0;
    }
    /* x and y are two fields: check them as one "x,y". */
    x_at = st[0];
    x_len = st[1] + ln[1] - st[0];
    if (!point(v + x_at, x_len, &p->at)) {
        return 0;
    }
    n = dx_strip(v + st[2], ln[2], &a);
    for (k = 0; k < (int)(sizeof(kinds) / sizeof(kinds[0])); k++) {
        if (dx_str_len(kinds[k]) == n) {
            int j = 0;

            while (j < n && kinds[k][j] == v[st[2] + a + j]) {
                j++;
            }
            if (j == n) {
                break;
            }
        }
    }
    if (k == (int)(sizeof(kinds) / sizeof(kinds[0]))) {
        return 0;
    }
    p->kind = (uint8_t)k;
    n = dx_strip(v + st[3], ln[3], &a);
    if (n == 0) {
        return 0;
    }
    dx_str_cut(p->name, sizeof(p->name), v + st[3] + a, n);
    return 1;
}

/* "map.local" -> 0, "map.world.lines" -> 1 and part "lines". */
static int which_map(const char *section, const char **part)
{
    static const char *const names[] = { "map.local", "map.world" };
    int m;

    for (m = 0; m < DX_MAPS_MAX; m++) {
        int n = dx_str_len(names[m]);
        int j = 0;

        while (j < n && section[j] == names[m][j]) {
            j++;
        }
        if (j == n && (section[n] == '\0' || section[n] == '.')) {
            *part = section[n] == '\0' ? "" : section + n + 1;
            return m;
        }
    }
    return -1;
}

static int on_key(const char *section, const char *key, const char *value, void *ctx)
{
    const char *part = "";
    int m = which_map(section, &part);
    map_stage_t *s;
    int n;

    (void)ctx;
    if (m < 0) {
        return 0;
    }
    s = &stage[m];
    if (part[0] == '\0') {
        s->head = 1;
        if (dx_str_eq(key, "title")) {
            dx_str_cut(s->title, sizeof(s->title), value, dx_str_len(value));
        } else if (dx_str_eq(key, "box")) {
            s->box_ok = (uint8_t)box(value, s->box);
        }
    } else if (dx_str_eq(part, "lines")) {
        n = dx_numbered_key(key, 'l', DX_LINES_MAX);
        if (n > 0) {
            s->lines[n - 1].on = (uint8_t)line(value, &s->lines[n - 1].line);
        }
    } else if (dx_str_eq(part, "places")) {
        n = dx_numbered_key(key, 'p', DX_PLACES_MAX);
        if (n > 0) {
            s->places[n - 1].on = (uint8_t)place(value, &s->places[n - 1].place);
        }
    }
    return 0;
}

static int inside(const int16_t b[4], dx_point_t p)
{
    return p.x >= b[0] && p.x <= b[2] && p.y >= b[1] && p.y <= b[3];
}

static void finish(dx_maps_t *out)
{
    int m;
    int i;
    int k;

    for (m = 0; m < DX_MAPS_MAX; m++) {
        const map_stage_t *s = &stage[m];
        dx_map_t *d;

        if (!s->head || !s->box_ok) {
            continue;
        }
        d = &out->maps[out->count++];
        d->which = m;
        if (s->title[0] != '\0') {
            dx_mem_copy(d->title, s->title, sizeof(d->title));
        } else {
            dx_str_cut(d->title, sizeof(d->title), m == 0 ? "local" : "world", 5);
        }
        for (i = 0; i < 4; i++) {
            d->box[i] = s->box[i];
        }
        for (i = 0; i < DX_LINES_MAX; i++) {
            const dx_polyline_t *l = &s->lines[i].line;
            int ok = s->lines[i].on;

            for (k = 0; ok && k < l->count; k++) {
                ok = inside(d->box, l->pts[k]);
            }
            if (ok) {
                d->lines[d->line_count++] = *l;
            }
        }
        for (i = 0; i < DX_PLACES_MAX; i++) {
            if (s->places[i].on && inside(d->box, s->places[i].place.at)) {
                d->places[d->place_count++] = s->places[i].place;
            }
        }
    }
}

void dx_map_load(dx_maps_t *m, const uint8_t *data, int32_t size)
{
    dx_load_t ld;

    dx_mem_set(m, 0, sizeof(*m));
    dx_mem_set(stage, 0, sizeof(stage));
    if (dx_load_ini(&ld, data, size, on_key, NULL)) {
        finish(m);
    }
    m->load = ld;
}

dx_map_fit_t dx_map_fit(const int16_t box[4], int x, int y, int w, int h)
{
    dx_map_fit_t f;
    int bw = box[2] - box[0];
    int bh = box[3] - box[1];
    int aw = w - 1;
    int ah = h - 1;

    f.x0 = box[0];
    f.y0 = box[1];
    f.ox = x;
    f.oy = y;
    f.num = 0;
    f.den = 1;
    if (w <= 0 || h <= 0 || bw <= 0 || bh <= 0) {
        return f;
    }
    /* The smaller of aw / bw and ah / bh, compared without division. */
    if (aw * bh <= ah * bw) {
        f.num = aw;
        f.den = bw;
    } else {
        f.num = ah;
        f.den = bh;
    }
    f.ox = x + (aw - bw * f.num / f.den) / 2;
    f.oy = y + (ah - bh * f.num / f.den) / 2;
    return f;
}

void dx_map_to_px(const dx_map_fit_t *f, int mx, int my, int *px, int *py)
{
    *px = f->ox + ((mx - f->x0) * f->num + f->den / 2) / f->den;
    *py = f->oy + ((my - f->y0) * f->num + f->den / 2) / f->den;
}
