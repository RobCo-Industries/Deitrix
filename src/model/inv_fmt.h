/*
 * Pure helpers of the INV tab: which item is row N of a category, and
 * the text of a count, a weight and a category's summary line.
 */
#ifndef DX_MODEL_INV_FMT_H
#define DX_MODEL_INV_FMT_H

#include <stdint.h>

#include "model/items.h"
#include "model/util.h"

/* Buffer sizes that hold the longest text each function writes, NUL
 * included: "999 " and a unit of 6; "63999.9 kg" for 64 items of
 * 999999 g; "64 entries, " and that weight. */
#define DX_INV_COUNT_CAP   12
#define DX_INV_WEIGHT_CAP  12
#define DX_INV_SUMMARY_CAP 32

/* The items of category `cat`, in file order. 0 for a bad category. */
int dx_inv_cat_entries(const dx_items_t *it, int cat);

/* The model index of row `row` of category `cat`, or -1. */
int dx_inv_item_at(const dx_items_t *it, int cat, int row);

/* The count and the unit: "12", "3 kg". */
void dx_inv_fmt_count(dx_sb_t *sb, const dx_item_t *item);

/* Grams as text: "760 g" below 1000 g, else kilograms with one decimal,
 * rounded half up to the nearest 100 g: 1949 g is "1.9 kg", 1950 g is
 * "2.0 kg". */
void dx_inv_fmt_weight(dx_sb_t *sb, uint32_t grams);

/* The summary of a category: "4 entries, 760 g", "1 entry, 90 g", or
 * only the entry count when no item of the category has a weight. */
void dx_inv_fmt_summary(dx_sb_t *sb, const dx_items_t *it, int cat);

#endif
