#include "model/radio.h"

/* ---- RADIO.INI ------------------------------------------------------------ */

typedef struct st_stage {
    uint8_t seen, has_name, has_kind, has_file, kind_ok;
    char name[DX_STATION_NAME_MAX + 1];
    uint8_t kind;
    uint8_t loop;
    char file[32];
} st_stage_t;

static st_stage_t stage[DX_STATIONS_MAX];

static int station_number(const char *section)
{
    static const char prefix[] = "station.";
    int i;

    for (i = 0; prefix[i] != '\0'; i++) {
        if (section[i] != prefix[i]) {
            return 0;
        }
    }
    if (section[i] < '1' || section[i] > '0' + DX_STATIONS_MAX || section[i + 1] != '\0') {
        return 0;
    }
    return section[i] - '0';
}

static int on_key(const char *section, const char *key, const char *value, void *ctx)
{
    int n = station_number(section);
    st_stage_t *s;
    int len = dx_str_len(value);

    (void)ctx;
    if (n == 0) {
        return 0;
    }
    s = &stage[n - 1];
    s->seen = 1;
    if (dx_str_eq(key, "name")) {
        s->has_name = len > 0;
        dx_str_cut(s->name, sizeof(s->name), value, len);
    } else if (dx_str_eq(key, "kind")) {
        s->has_kind = 1;
        s->kind_ok = dx_str_eq(value, "tune") || dx_str_eq(value, "pcm");
        s->kind = dx_str_eq(value, "pcm") ? DX_KIND_PCM : DX_KIND_TUNE;
    } else if (dx_str_eq(key, "file")) {
        s->has_file = len < (int)sizeof(s->file);
        dx_str_cut(s->file, sizeof(s->file), value, len);
    } else if (dx_str_eq(key, "loop")) {
        s->loop = !dx_str_eq(value, "no");
    }
    return 0;
}

void dx_radio_load(dx_radio_t *r, const uint8_t *data, int32_t size)
{
    dx_load_t ld;
    int i;

    dx_mem_set(r, 0, sizeof(*r));
    dx_mem_set(stage, 0, sizeof(stage));
    for (i = 0; i < DX_STATIONS_MAX; i++) {
        stage[i].loop = 1;
    }
    if (dx_load_ini(&ld, data, size, on_key, NULL)) {
        for (i = 0; i < DX_STATIONS_MAX; i++) {
            const st_stage_t *s = &stage[i];
            dx_station_t *d;

            if (!s->seen || !s->has_name || !s->has_kind || !s->kind_ok || !s->has_file ||
                !dx_is_name83(s->file) ||
                !dx_str_ends(s->file, s->kind == DX_KIND_PCM ? ".PCM" : ".TUN")) {
                continue;
            }
            d = &r->stations[r->count++];
            d->number = i + 1;
            dx_mem_copy(d->name, s->name, sizeof(d->name));
            d->kind = s->kind;
            d->loop = s->kind == DX_KIND_PCM ? s->loop : 1;
            dx_str_cut(d->file, sizeof(d->file), s->file, dx_str_len(s->file));
            d->file_load.status = DX_LOAD_NONE;
        }
    }
    r->load = ld;
}

int dx_radio_find(const dx_radio_t *r, int number)
{
    int i;

    for (i = 0; i < r->count; i++) {
        if (r->stations[i].number == number) {
            return i;
        }
    }
    return -1;
}

void dx_radio_set_tune(dx_radio_t *r, int i, const uint8_t *data, int32_t size)
{
    dx_station_t *s;
    unsigned line = 0;

    if (i < 0 || i >= r->count || r->stations[i].kind != DX_KIND_TUNE) {
        return;
    }
    s = &r->stations[i];
    s->playable = 0;
    s->note_count = 0;
    if (size == DX_FILE_TOO_LARGE || (size >= 0 && size > DX_TUN_LIMIT)) {
        s->file_load.status = DX_LOAD_TOO_LARGE;
        s->file_load.line = 0;
        return;
    }
    if (!dx_load_size(&s->file_load, size)) {
        return;
    }
    if (!dx_tun_parse(data, size, s->notes, &s->note_count, &s->tune_ms, &line)) {
        s->file_load.status = DX_LOAD_BROKEN;
        s->file_load.line = line;
        s->note_count = 0;
        return;
    }
    s->playable = 1;
}

void dx_radio_set_pcm(dx_radio_t *r, int i, int32_t size)
{
    dx_station_t *s;

    if (i < 0 || i >= r->count || r->stations[i].kind != DX_KIND_PCM) {
        return;
    }
    s = &r->stations[i];
    s->playable = 0;
    if (!dx_load_size(&s->file_load, size)) {
        return;
    }
    s->pcm_size = (uint32_t)size;
    if ((uint32_t)size > DX_PCM_MAX) {
        s->file_load.status = DX_LOAD_TOO_LARGE;
    } else if ((uint32_t)size < DX_PCM_MIN) {
        s->file_load.status = DX_LOAD_TOO_SMALL;
    } else {
        s->playable = 1;
    }
}

/* ---- .TUN: tools/tun2notes.py in C ------------------------------------------ */

#define TEMPO_MIN 30
#define TEMPO_MAX 300
#define FREQ_MIN  20
#define FREQ_MAX  20000

static const uint16_t octave8[12] = {
    4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902,
};

typedef struct tok {
    const char *s;
    int n;
} tok_t;

static int tok_is(tok_t t, const char *w)
{
    int i;

    for (i = 0; i < t.n; i++) {
        if (w[i] != t.s[i]) {
            return 0;
        }
    }
    return w[t.n] == '\0';
}

/* 1 to 5 decimal digits, as the tool's isdigit() and len() <= 5. */
static int digits(const char *s, int n, uint32_t *v)
{
    int i;

    if (n < 1 || n > 5) {
        return 0;
    }
    *v = 0;
    for (i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        *v = *v * 10u + (uint32_t)(s[i] - '0');
    }
    return 1;
}

static int length_ok(uint32_t l)
{
    return l == 1 || l == 2 || l == 4 || l == 8 || l == 16 || l == 32;
}

/* The frequency of a pitch: 0 for a rest. -1 when it is bad. */
static int32_t pitch(const char *s, int n)
{
    static const int8_t steps[7] = { 9, 11, 0, 2, 4, 5, 7 }; /* A to G */
    uint32_t f;
    int step;
    int octave;
    int acc = 0;
    int shift;

    if (n == 1 && s[0] == 'R') {
        return 0;
    }
    if (n >= 2 && s[n - 2] == 'h' && s[n - 1] == 'z') {
        if (!digits(s, n - 2, &f) || f < FREQ_MIN || f > FREQ_MAX) {
            return -1;
        }
        return (int32_t)f;
    }
    if (n < 2 || s[0] < 'A' || s[0] > 'G') {
        return -1;
    }
    if (s[1] == '#' || s[1] == 'b') {
        acc = s[1] == '#' ? 1 : -1;
    }
    if (n != 2 + (acc != 0) || s[n - 1] < '0' || s[n - 1] > '9') {
        return -1;
    }
    step = steps[s[0] - 'A'] + acc;
    octave = s[n - 1] - '0';
    if (step < 0) {
        step += 12;
        octave--;
    } else if (step > 11) {
        step -= 12;
        octave++;
    }
    if (octave < 1 || octave > 8) {
        return -1;
    }
    shift = 8 - octave;
    f = octave8[step];
    return (int32_t)(shift == 0 ? f : (f + (1u << (shift - 1))) >> shift);
}

static uint32_t duration_ms(uint32_t tempo, uint32_t length, int dotted)
{
    return 240000u * (dotted ? 3u : 2u) / (tempo * length * 2u);
}

int dx_tun_parse(const uint8_t *data, int32_t size, dx_note_t *out, int *count,
                 uint32_t *total_ms, unsigned *err_line)
{
    const char *text = (const char *)data;
    uint32_t tempo = 120;
    uint32_t length = 4;
    int notes = 0;      /* may pass 256: only the first 256 are stored */
    int rep_start = -1; /* the note index where |: opened */
    int rep_used = 0;
    int seen_version = 0;
    int32_t pos = 0;
    unsigned line = 0;
    unsigned lines;
    uint32_t total = 0;
    int i;

    *count = 0;
    *total_ms = 0;
    *err_line = 0;
    if (size < 0 || size > DX_TUN_LIMIT) {
        return 0;
    }
    /* The tool's line count: split at LF, without an empty last piece. */
    lines = 1;
    for (i = 0; i < size; i++) {
        lines += text[i] == '\n';
    }
    if (size == 0 || text[size - 1] == '\n') {
        lines--;
    }
    while (pos < size) {
        tok_t toks[DX_TUN_LINE_MAX];
        int ntok = 0;
        int32_t end = pos;
        int32_t stop;
        int t;

        line++;
        while (end < size && text[end] != '\n') {
            end++;
        }
        stop = end;
        if (stop > pos && text[stop - 1] == '\r') {
            stop--;
        }
        if (stop - pos > DX_TUN_LINE_MAX) {
            *err_line = line;
            return 0;
        }
        for (i = pos; i < stop; i++) {
            uint8_t c = (uint8_t)text[i];

            if ((c < 0x20 && c != '\t') || c > 0x7e) {
                *err_line = line;
                return 0;
            }
        }
        /* ';' starts a comment. Tokens are split on spaces and tabs. */
        for (i = pos; i < stop && text[i] != ';';) {
            if (text[i] == ' ' || text[i] == '\t') {
                i++;
                continue;
            }
            toks[ntok].s = text + i;
            while (i < stop && text[i] != ';' && text[i] != ' ' && text[i] != '\t') {
                i++;
            }
            toks[ntok].n = (int)(text + i - toks[ntok].s);
            ntok++;
        }
        pos = end + 1;
        if (ntok == 0) {
            continue;
        }
        if (!seen_version) {
            if (ntok != 2 || !tok_is(toks[0], "version") || !tok_is(toks[1], "1")) {
                *err_line = line;
                return 0;
            }
            seen_version = 1;
            continue;
        }
        for (t = 0; t < ntok; t++) {
            tok_t k = toks[t];
            int slash = -1;
            int32_t f;
            uint32_t ln = length;
            int dotted = 0;

            if (tok_is(k, "tempo") || tok_is(k, "length")) {
                uint32_t v;

                if (t + 1 >= ntok || !digits(toks[t + 1].s, toks[t + 1].n, &v)) {
                    *err_line = line;
                    return 0;
                }
                if (tok_is(k, "tempo")) {
                    if (v < TEMPO_MIN || v > TEMPO_MAX) {
                        *err_line = line;
                        return 0;
                    }
                    tempo = v;
                } else {
                    if (!length_ok(v)) {
                        *err_line = line;
                        return 0;
                    }
                    length = v;
                }
                t++;
                continue;
            }
            if (tok_is(k, "version")) {
                *err_line = line;
                return 0;
            }
            if (tok_is(k, "|:")) {
                if (rep_used || rep_start >= 0) {
                    *err_line = line;
                    return 0;
                }
                rep_start = notes;
                continue;
            }
            if (tok_is(k, ":|")) {
                int start = rep_start >= 0 ? rep_start : 0;
                int j;

                if (rep_used || start == notes) {
                    *err_line = line;
                    return 0;
                }
                for (j = start; j < notes && notes + (j - start) < DX_TUN_NOTES_MAX; j++) {
                    out[notes + (j - start)] = out[j];
                }
                notes += notes - start;
                rep_start = -1;
                rep_used = 1;
                continue;
            }
            for (i = 0; i < k.n; i++) {
                if (k.s[i] == '/') {
                    slash = i;
                    break;
                }
            }
            f = pitch(k.s, slash >= 0 ? slash : k.n);
            if (f < 0) {
                *err_line = line;
                return 0;
            }
            if (slash >= 0) {
                const char *d = k.s + slash + 1;
                int dn = k.n - slash - 1;

                if (dn > 0 && d[dn - 1] == '.') {
                    dn--;
                    dotted = 1;
                }
                if (!digits(d, dn, &ln) || !length_ok(ln)) {
                    *err_line = line;
                    return 0;
                }
            }
            if (notes >= DX_TUN_NOTES_MAX) {
                *err_line = line; /* more than 256 notes */
                return 0;
            }
            out[notes].freq_hz = (uint16_t)f;
            out[notes].duration_ms = (uint16_t)duration_ms(tempo, ln, dotted);
            notes++;
        }
    }
    if (!seen_version) {
        *err_line = 1;
        return 0;
    }
    if (rep_start >= 0 || notes == 0 || notes > DX_TUN_NOTES_MAX) {
        *err_line = lines;
        return 0;
    }
    for (i = 0; i < notes; i++) {
        total += out[i].duration_ms;
    }
    if (total > DX_TUN_MS_MAX) {
        *err_line = lines;
        return 0;
    }
    *count = notes;
    *total_ms = total;
    return 1;
}
