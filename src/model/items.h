/*
 * ITEMS.INI: the categories and items of the INV tab, and the overlay
 * the owner changes: a count and a marked flag per item, kept in INV.SAV.
 */
#ifndef DX_MODEL_ITEMS_H
#define DX_MODEL_ITEMS_H

#include <stdint.h>

#include "model/load.h"

#define DX_ITEMS_LIMIT     32768
#define DX_CATS_MAX        5
#define DX_ITEMS_MAX       64
#define DX_ID_MAX          12
#define DX_CAT_LABEL_MAX   5
#define DX_ITEM_NAME_MAX   30
#define DX_COUNT_MAX       999
#define DX_UNIT_MAX        6
#define DX_WEIGHT_MAX      999999u
#define DX_WEIGHT_NONE     0xffffffffu
#define DX_NOTE_MAX        200
#define DX_NOTE_PART_MAX   127

typedef struct dx_category {
    char key[DX_ID_MAX + 1];
    char label[DX_CAT_LABEL_MAX + 1];
} dx_category_t;

typedef struct dx_item {
    char id[DX_ID_MAX + 1];
    char name[DX_ITEM_NAME_MAX + 1];
    char unit[DX_UNIT_MAX + 1];       /* empty: no unit */
    char note[DX_NOTE_MAX + 1];       /* note, a space, note2; empty: no note */
    uint8_t cat;                      /* index in cats */
    uint16_t base_count;              /* the count in ITEMS.INI */
    uint32_t weight;                  /* grams, or DX_WEIGHT_NONE */
    /* The overlay. */
    uint16_t count;                   /* 0 to 999 */
    uint8_t marked;
} dx_item_t;

typedef struct dx_items {
    dx_load_t load;
    dx_category_t cats[DX_CATS_MAX];
    int cat_count;
    dx_item_t items[DX_ITEMS_MAX];    /* in file order */
    int item_count;
    uint32_t changes;                 /* counts overlay changes, for the saves */
} dx_items_t;

void dx_items_load(dx_items_t *it, const uint8_t *data, int32_t size);

/* The index of the item with this id, or -1. */
int dx_items_find(const dx_items_t *it, const char *id);

/* Overlay changes. Each returns 1 when the value changed, else 0, and
 * counts a change only then. Counts clamp to 0 to 999. A bad index does
 * nothing. */
int dx_items_set_count(dx_items_t *it, int index, int32_t count);
int dx_items_add(dx_items_t *it, int index, int32_t delta);
int dx_items_toggle_marked(dx_items_t *it, int index);

/* Totals of a category for the screens: its entries, and the sum of the
 * weights the file gives, in grams. Items without a weight add nothing.
 * The weight is the item's as written: a changed count does not scale it.
 * `weighed` is 1 when at least one item of the category has a weight. */
typedef struct dx_cat_total {
    int entries;
    uint32_t grams;
    int weighed;
} dx_cat_total_t;
dx_cat_total_t dx_items_total(const dx_items_t *it, int cat);

#endif
