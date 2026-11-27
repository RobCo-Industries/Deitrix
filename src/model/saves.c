#include "model/saves.h"

#include "platform/platform.h"

#define VERSION 1

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v);
    put16(p + 2, v >> 16);
}

static uint32_t get16(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t get32(const uint8_t *p)
{
    return get16(p) | get16(p + 2) << 16;
}

static void header(uint8_t *out, const char *magic, uint32_t count, uint32_t payload)
{
    int i;

    for (i = 0; i < 4; i++) {
        out[i] = (uint8_t)magic[i];
    }
    put16(out + 4, VERSION);
    put16(out + 6, count);
    put32(out + 8, dx_crc32(out + DX_SAVE_HEADER, payload));
}

/* Checks the header, the size and the CRC. *count gets the records. */
static int check(const uint8_t *buf, int32_t n, const char *magic, uint32_t record,
                 uint32_t max_count, uint32_t *count)
{
    int i;

    if (n == DX_FILE_MISSING) {
        return DX_SAVE_MISSING;
    }
    if (n < 0) {
        return DX_SAVE_READ_ERROR;
    }
    if (n < DX_SAVE_HEADER) {
        return DX_SAVE_BAD_SIZE;
    }
    for (i = 0; i < 4; i++) {
        if (buf[i] != (uint8_t)magic[i]) {
            return DX_SAVE_BAD_MAGIC;
        }
    }
    if (get16(buf + 4) != VERSION) {
        return DX_SAVE_BAD_VERSION;
    }
    *count = get16(buf + 6);
    if (*count > max_count || (uint32_t)n != DX_SAVE_HEADER + record * *count) {
        return DX_SAVE_BAD_SIZE;
    }
    if (get32(buf + 8) != dx_crc32(buf + DX_SAVE_HEADER, (uint32_t)n - DX_SAVE_HEADER)) {
        return DX_SAVE_BAD_CRC;
    }
    return DX_SAVE_OK;
}

const char *dx_save_reason(int status)
{
    switch (status) {
    case DX_SAVE_OK:
        return "ok";
    case DX_SAVE_MISSING:
        return "missing";
    case DX_SAVE_READ_ERROR:
        return "ignored: read error";
    case DX_SAVE_BAD_MAGIC:
        return "ignored: bad magic";
    case DX_SAVE_BAD_VERSION:
        return "ignored: bad version";
    case DX_SAVE_BAD_SIZE:
        return "ignored: bad size";
    default:
        return "ignored: bad crc";
    }
}

/* ---- DEITRIX.SAV ----------------------------------------------------------- */

void dx_settings_defaults(dx_settings_t *s, uint8_t volume)
{
    s->last_tab = 0;
    s->volume = volume > DX_VOLUME_MAX ? DX_VOLUME_MAX : volume;
    s->ui_sound = 1;
    s->last_station = 0;
}

static int set(dx_settings_t *s, uint8_t *field, int v, int max)
{
    if (v < 0 || v > max || *field == (uint8_t)v) {
        return 0;
    }
    *field = (uint8_t)v;
    s->changes++;
    return 1;
}

int dx_settings_set_tab(dx_settings_t *s, int tab)
{
    return set(s, &s->last_tab, tab, DX_TABS - 1);
}

int dx_settings_set_volume(dx_settings_t *s, int volume)
{
    return set(s, &s->volume, volume, DX_VOLUME_MAX);
}

int dx_settings_set_ui_sound(dx_settings_t *s, int level)
{
    return set(s, &s->ui_sound, level, 3);
}

int dx_settings_set_station(dx_settings_t *s, int number)
{
    return set(s, &s->last_station, number, 6);
}

void dx_settings_encode(const dx_settings_t *s, uint8_t out[DX_SETTINGS_SIZE])
{
    uint8_t *r = out + DX_SAVE_HEADER;

    r[0] = s->last_tab;
    r[1] = s->volume;
    r[2] = s->ui_sound;
    r[3] = s->last_station;
    put32(r + 4, 0);
    header(out, "DXST", 1, DX_SETTINGS_SIZE - DX_SAVE_HEADER);
}

int dx_settings_decode(dx_settings_t *s, const uint8_t *buf, int32_t n, uint8_t volume)
{
    uint32_t count = 0;
    int st;
    const uint8_t *r;

    dx_settings_defaults(s, volume);
    st = check(buf, n, "DXST", DX_SETTINGS_SIZE - DX_SAVE_HEADER, 1, &count);
    if (st != DX_SAVE_OK || count != 1) {
        return st != DX_SAVE_OK ? st : DX_SAVE_BAD_SIZE;
    }
    r = buf + DX_SAVE_HEADER;
    if (r[0] < DX_TABS) {
        s->last_tab = r[0];
    }
    if (r[1] <= DX_VOLUME_MAX) {
        s->volume = r[1];
    }
    if (r[2] <= 3) {
        s->ui_sound = r[2];
    }
    if (r[3] <= 6) {
        s->last_station = r[3];
    }
    return DX_SAVE_OK;
}

/* ---- INV.SAV --------------------------------------------------------------- */

int dx_inv_encode(const dx_items_t *it, uint8_t out[DX_INV_SAVE_MAX])
{
    int i;
    int k;

    for (i = 0; i < it->item_count; i++) {
        uint8_t *r = out + DX_SAVE_HEADER + DX_INV_RECORD * i;
        const dx_item_t *d = &it->items[i];

        for (k = 0; k < DX_ID_MAX; k++) {
            r[k] = (uint8_t)d->id[k]; /* the id's NUL pads the rest */
            if (d->id[k] == '\0') {
                break;
            }
        }
        for (; k < DX_ID_MAX; k++) {
            r[k] = 0;
        }
        put16(r + 12, d->count);
        r[14] = d->marked ? 1 : 0;
        r[15] = 0;
    }
    header(out, "DXIV", (uint32_t)it->item_count, (uint32_t)(DX_INV_RECORD * it->item_count));
    return DX_SAVE_HEADER + DX_INV_RECORD * it->item_count;
}

/* The record's id as a string, or 0 when it is not NUL-padded. */
static int record_id(const uint8_t *r, char id[DX_ID_MAX + 1])
{
    int k = 0;

    while (k < DX_ID_MAX && r[k] != 0) {
        id[k] = (char)r[k];
        k++;
    }
    id[k] = '\0';
    for (; k < DX_ID_MAX; k++) {
        if (r[k] != 0) {
            return 0;
        }
    }
    return 1;
}

int dx_inv_decode(dx_items_t *it, const uint8_t *buf, int32_t n, int *applied)
{
    uint8_t seen[DX_ITEMS_MAX];
    uint32_t count = 0;
    uint32_t i;
    int st;

    *applied = 0;
    for (i = 0; i < (uint32_t)it->item_count; i++) {
        it->items[i].count = it->items[i].base_count;
        it->items[i].marked = 0;
        seen[i] = 0;
    }
    st = check(buf, n, "DXIV", DX_INV_RECORD, DX_ITEMS_MAX, &count);
    if (st != DX_SAVE_OK) {
        return st;
    }
    for (i = 0; i < count; i++) {
        const uint8_t *r = buf + DX_SAVE_HEADER + DX_INV_RECORD * i;
        char id[DX_ID_MAX + 1];
        int x;
        uint32_t c;

        if (!record_id(r, id) || (x = dx_items_find(it, id)) < 0 || seen[x]) {
            continue;
        }
        seen[x] = 1;
        (*applied)++;
        c = get16(r + 12);
        if (c <= DX_COUNT_MAX) {
            it->items[x].count = (uint16_t)c;
        }
        if (r[14] <= 1) {
            it->items[x].marked = r[14];
        }
    }
    return DX_SAVE_OK;
}
