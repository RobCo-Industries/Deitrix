#include "model/items.h"

#define CAT_SLOTS (DX_CATS_MAX)
#define NAME_RAW  128

typedef struct cat_stage {
    char key[DX_ID_MAX + 1];
    int label_ok;
    char label[DX_CAT_LABEL_MAX + 1];
} cat_stage_t;

typedef struct item_stage {
    char id[DX_ID_MAX + 1];
    uint8_t has_name, has_cat, has_note, has_note2;
    char name[DX_ITEM_NAME_MAX + 1];
    char cat[NAME_RAW];
    uint16_t count;
    char unit[DX_UNIT_MAX + 1];
    uint32_t weight;
    char note[DX_NOTE_PART_MAX + 1];
    char note2[DX_NOTE_PART_MAX + 1];
} item_stage_t;

typedef struct staging {
    cat_stage_t cats[CAT_SLOTS];
    int cat_count;
    item_stage_t items[DX_ITEMS_MAX];
    int item_count;
} staging_t;

static staging_t stage;

static int label_ok(const char *s)
{
    int n = 0;

    for (; s[n] != '\0'; n++) {
        if (s[n] < 'A' || s[n] > 'Z' || n >= DX_CAT_LABEL_MAX) {
            return 0;
        }
    }
    return n >= 1;
}

static void category(const char *key, const char *value)
{
    int i;
    cat_stage_t *c = NULL;

    if (!dx_is_id(key)) {
        return;
    }
    for (i = 0; i < stage.cat_count; i++) {
        if (dx_str_eq(stage.cats[i].key, key)) {
            c = &stage.cats[i];
        }
    }
    if (c == NULL) {
        if (stage.cat_count >= CAT_SLOTS) {
            return; /* the 6th and later */
        }
        c = &stage.cats[stage.cat_count++];
        dx_str_cut(c->key, sizeof(c->key), key, dx_str_len(key));
    }
    c->label_ok = label_ok(value);
    if (c->label_ok) {
        dx_str_cut(c->label, sizeof(c->label), value, dx_str_len(value));
    }
}

static item_stage_t *item_for(const char *id)
{
    int i;
    item_stage_t *s;

    for (i = 0; i < stage.item_count; i++) {
        if (dx_str_eq(stage.items[i].id, id)) {
            return &stage.items[i];
        }
    }
    if (stage.item_count >= DX_ITEMS_MAX) {
        return NULL; /* the 65th and later */
    }
    s = &stage.items[stage.item_count++];
    dx_mem_set(s, 0, sizeof(*s));
    dx_str_cut(s->id, sizeof(s->id), id, dx_str_len(id));
    s->count = 1;
    s->weight = DX_WEIGHT_NONE;
    return s;
}

static void item_key(item_stage_t *s, const char *key, const char *value)
{
    int n = dx_str_len(value);
    uint32_t v;

    if (dx_str_eq(key, "name")) {
        s->has_name = n > 0;
        dx_str_cut(s->name, sizeof(s->name), value, n);
    } else if (dx_str_eq(key, "cat")) {
        s->has_cat = 1;
        dx_str_cut(s->cat, sizeof(s->cat), value, n);
    } else if (dx_str_eq(key, "count")) {
        if (!dx_parse_num(value, n, &v)) {
            v = 1;
        }
        s->count = (uint16_t)(v > DX_COUNT_MAX ? DX_COUNT_MAX : v);
    } else if (dx_str_eq(key, "unit")) {
        dx_str_cut(s->unit, sizeof(s->unit), value, n);
    } else if (dx_str_eq(key, "weight")) {
        s->weight = dx_parse_num(value, n, &v) && v <= DX_WEIGHT_MAX ? v : DX_WEIGHT_NONE;
    } else if (dx_str_eq(key, "note")) {
        s->has_note = n > 0;
        dx_str_cut(s->note, sizeof(s->note), value, n);
    } else if (dx_str_eq(key, "note2")) {
        s->has_note2 = n > 0;
        dx_str_cut(s->note2, sizeof(s->note2), value, n);
    }
}

static int on_key(const char *section, const char *key, const char *value, void *ctx)
{
    (void)ctx;
    if (dx_str_eq(section, "categories")) {
        category(key, value);
    } else if (section[0] == 'i' && section[1] == 't' && section[2] == 'e' &&
               section[3] == 'm' && section[4] == '.' && dx_is_id(section + 5)) {
        item_stage_t *s = item_for(section + 5);

        if (s != NULL) {
            item_key(s, key, value);
        }
    }
    return 0;
}

/* The final category index of a key, or -1. */
static int cat_index(const dx_items_t *it, const char *key)
{
    int i;

    for (i = 0; i < it->cat_count; i++) {
        if (dx_str_eq(it->cats[i].key, key)) {
            return i;
        }
    }
    return -1;
}

static void finish(dx_items_t *it)
{
    int i;

    for (i = 0; i < stage.cat_count; i++) {
        if (stage.cats[i].label_ok) {
            dx_category_t *c = &it->cats[it->cat_count++];

            dx_mem_copy(c->key, stage.cats[i].key, sizeof(c->key));
            dx_mem_copy(c->label, stage.cats[i].label, sizeof(c->label));
        }
    }
    for (i = 0; i < stage.item_count; i++) {
        const item_stage_t *s = &stage.items[i];
        int cat = s->has_cat ? cat_index(it, s->cat) : -1;
        dx_item_t *d;
        dx_sb_t sb;

        if (!s->has_name || cat < 0) {
            continue;
        }
        d = &it->items[it->item_count++];
        dx_mem_copy(d->id, s->id, sizeof(d->id));
        dx_mem_copy(d->name, s->name, sizeof(d->name));
        dx_mem_copy(d->unit, s->unit, sizeof(d->unit));
        dx_sb_init(&sb, d->note, sizeof(d->note));
        if (s->has_note) {
            dx_sb_str(&sb, s->note);
            if (s->has_note2) {
                dx_sb_char(&sb, ' ');
                dx_sb_str(&sb, s->note2);
            }
        }
        d->cat = (uint8_t)cat;
        d->base_count = s->count;
        d->count = s->count;
        d->weight = s->weight;
        d->marked = 0;
    }
}

void dx_items_load(dx_items_t *it, const uint8_t *data, int32_t size)
{
    dx_load_t ld;

    dx_mem_set(it, 0, sizeof(*it));
    dx_mem_set(&stage, 0, sizeof(stage));
    if (dx_load_ini(&ld, data, size, on_key, NULL)) {
        finish(it);
    }
    it->load = ld;
}

int dx_items_find(const dx_items_t *it, const char *id)
{
    int i;

    for (i = 0; i < it->item_count; i++) {
        if (dx_str_eq(it->items[i].id, id)) {
            return i;
        }
    }
    return -1;
}

int dx_items_set_count(dx_items_t *it, int index, int32_t count)
{
    dx_item_t *d;

    if (index < 0 || index >= it->item_count) {
        return 0;
    }
    if (count < 0) {
        count = 0;
    } else if (count > DX_COUNT_MAX) {
        count = DX_COUNT_MAX;
    }
    d = &it->items[index];
    if (d->count == (uint16_t)count) {
        return 0;
    }
    d->count = (uint16_t)count;
    it->changes++;
    return 1;
}

int dx_items_add(dx_items_t *it, int index, int32_t delta)
{
    int32_t c;

    if (index < 0 || index >= it->item_count) {
        return 0;
    }
    /* Clamp the delta first so the sum cannot overflow. */
    if (delta > DX_COUNT_MAX) {
        delta = DX_COUNT_MAX;
    } else if (delta < -DX_COUNT_MAX) {
        delta = -DX_COUNT_MAX;
    }
    c = (int32_t)it->items[index].count + delta;
    return dx_items_set_count(it, index, c);
}

int dx_items_toggle_marked(dx_items_t *it, int index)
{
    if (index < 0 || index >= it->item_count) {
        return 0;
    }
    it->items[index].marked = (uint8_t)!it->items[index].marked;
    it->changes++;
    return 1;
}

dx_cat_total_t dx_items_total(const dx_items_t *it, int cat)
{
    dx_cat_total_t t;
    int i;

    t.entries = 0;
    t.grams = 0;
    t.weighed = 0;
    for (i = 0; i < it->item_count; i++) {
        if (it->items[i].cat == cat) {
            t.entries++;
            if (it->items[i].weight != DX_WEIGHT_NONE) {
                t.grams += it->items[i].weight; /* at most 64 * 999999 */
                t.weighed = 1;
            }
        }
    }
    return t;
}
