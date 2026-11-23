/* The INV tab's pure helpers: row mapping, counts, weights, summaries. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "fake_platform.h"
#include "model/inv_fmt.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static dx_items_t it;
static char text[DX_ITEMS_LIMIT];

static void load(const char *t)
{
    dx_items_load(&it, (const uint8_t *)t, (int32_t)strlen(t));
    CHECK_EQ(it.load.status, DX_LOAD_OK);
}

static const char *weight(uint32_t g)
{
    static char buf[DX_INV_WEIGHT_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, buf, sizeof(buf));
    dx_sb_str(&sb, "");
    dx_inv_fmt_weight(&sb, g);
    return buf;
}

static const char *summary(int cat)
{
    static char buf[DX_INV_SUMMARY_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, buf, sizeof(buf));
    dx_inv_fmt_summary(&sb, &it, cat);
    return buf;
}

static const char *count(int i)
{
    static char buf[DX_INV_COUNT_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, buf, sizeof(buf));
    dx_inv_fmt_count(&sb, &it.items[i]);
    return buf;
}

TEST(weights)
{
    CHECK_STR(weight(0), "0 g");
    CHECK_STR(weight(90), "90 g");
    CHECK_STR(weight(999), "999 g");
    CHECK_STR(weight(1000), "1.0 kg");
    CHECK_STR(weight(1049), "1.0 kg");
    CHECK_STR(weight(1050), "1.1 kg");
    CHECK_STR(weight(1949), "1.9 kg");
    CHECK_STR(weight(1950), "2.0 kg");
    CHECK_STR(weight(9999), "10.0 kg");
    CHECK_STR(weight(999999), "1000.0 kg");
    /* The largest total: 64 items of 999999 g. */
    CHECK_STR(weight(64u * 999999u), "63999.9 kg");
    CHECK_INT(strlen(weight(64u * 999999u)) + 1, DX_INV_WEIGHT_CAP - 1);
}

TEST(counts_and_rows)
{
    load("version = 1\n[categories]\na = AAA\nb = BBB\nc = CCC\n"
         "[item.x]\nname = X\ncat = b\ncount = 12\n"
         "[item.y]\nname = Y\ncat = a\ncount = 3\nunit = kg\n"
         "[item.z]\nname = Z\ncat = b\ncount = 999\nunit = abcdef\n"
         "[item.w]\nname = W\ncat = b\ncount = 0\n");
    CHECK_STR(count(0), "12");
    CHECK_STR(count(1), "3 kg");
    CHECK_STR(count(2), "999 abcdef");
    CHECK_INT(strlen(count(2)) + 1, DX_INV_COUNT_CAP - 1);
    CHECK_STR(count(3), "0");
    CHECK_INT(dx_inv_cat_entries(&it, 0), 1);
    CHECK_INT(dx_inv_cat_entries(&it, 1), 3);
    CHECK_INT(dx_inv_cat_entries(&it, 2), 0);
    CHECK_INT(dx_inv_cat_entries(&it, 3), 0);
    CHECK_INT(dx_inv_cat_entries(&it, -1), 0);
    CHECK_INT(dx_inv_item_at(&it, 0, 0), 1);
    CHECK_INT(dx_inv_item_at(&it, 0, 1), -1);
    CHECK_INT(dx_inv_item_at(&it, 1, 0), 0);
    CHECK_INT(dx_inv_item_at(&it, 1, 1), 2);
    CHECK_INT(dx_inv_item_at(&it, 1, 2), 3);
    CHECK_INT(dx_inv_item_at(&it, 1, 3), -1);
    CHECK_INT(dx_inv_item_at(&it, 1, -1), -1);
    CHECK_INT(dx_inv_item_at(&it, 2, 0), -1);
    CHECK_INT(dx_inv_item_at(&it, 5, 0), -1);
}

TEST(summaries)
{
    load("version = 1\n[categories]\na = AAA\nb = BBB\nc = CCC\nd = DDD\n"
         "[item.x]\nname = X\ncat = a\nweight = 90\n"
         "[item.y]\nname = Y\ncat = b\n"
         "[item.z]\nname = Z\ncat = b\n"
         "[item.v]\nname = V\ncat = c\nweight = 900\n"
         "[item.u]\nname = U\ncat = c\nweight = 150\n"
         "[item.t]\nname = T\ncat = c\n"
         "[item.s]\nname = S\ncat = d\nweight = 0\n");
    CHECK_STR(summary(0), "1 entry, 90 g");
    CHECK_STR(summary(1), "2 entries");
    CHECK_STR(summary(2), "3 entries, 1.1 kg");
    /* A weight of 0 is a weight. */
    CHECK_STR(summary(3), "1 entry, 0 g");
    /* A changed count does not scale the weight. */
    CHECK(dx_items_set_count(&it, 3, 7));
    CHECK_STR(summary(2), "3 entries, 1.1 kg");
}

TEST(summary_at_the_limits)
{
    int n = 0;
    int i;

    n += tfmt(text + n, sizeof(text) - (size_t)n, "version = 1\n[categories]\na = A\n");
    for (i = 0; i < 64; i++) {
        n += tfmt(text + n, sizeof(text) - (size_t)n,
                  "[item.i%d]\nname = Item %d\ncat = a\nweight = 999999\n", i, i);
    }
    load(text);
    CHECK_INT(it.item_count, 64);
    CHECK_STR(summary(0), "64 entries, 63999.9 kg");
    CHECK(strlen(summary(0)) < DX_INV_SUMMARY_CAP);
    CHECK_INT(dx_inv_item_at(&it, 0, 63), 63);
    CHECK_INT(dx_inv_item_at(&it, 0, 64), -1);
}
