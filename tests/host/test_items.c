/* ITEMS.INI and the overlay: one case per rule of docs/CONTENT.md. */
#include "rc_test.h"
#include "tfmt.h"

#include <stdio.h>
#include <string.h>

#include "fake_platform.h"
#include "model/items.h"

static dx_items_t it;
static char text[DX_ITEMS_LIMIT];

static void load(const char *t)
{
    dx_items_load(&it, (const uint8_t *)t, (int32_t)strlen(t));
}

#define HEAD "version = 1\n[categories]\ntools = TOOLS\nfood = FOOD\n"

TEST(basic)
{
    load(HEAD "[item.knife]\nname = Folding knife\ncat = tools\ncount = 3\nunit = pc\n"
              "weight = 90\nnote = Keep it dry\nnote2 = and oiled.\n"
              "[item.water]\nname = Water\ncat = food\n");
    CHECK_EQ(it.load.status, DX_LOAD_OK);
    CHECK_EQ(it.cat_count, 2);
    CHECK_STR(it.cats[1].label, "FOOD");
    CHECK_EQ(it.item_count, 2);
    CHECK_STR(it.items[0].id, "knife");
    CHECK_STR(it.items[0].name, "Folding knife");
    CHECK_EQ(it.items[0].count, 3);
    CHECK_EQ(it.items[0].base_count, 3);
    CHECK_STR(it.items[0].unit, "pc");
    CHECK_EQ(it.items[0].weight, 90);
    CHECK_STR(it.items[0].note, "Keep it dry and oiled.");
    CHECK_EQ(it.items[1].cat, 1);
    /* Defaults: count 1, no unit, no weight, no note. */
    CHECK_EQ(it.items[1].count, 1);
    CHECK_STR(it.items[1].unit, "");
    CHECK_EQ(it.items[1].weight, DX_WEIGHT_NONE);
    CHECK_STR(it.items[1].note, "");
}

TEST(categories)
{
    /* Bad key: dropped. Bad label: dropped with its items. */
    load("version = 1\n[categories]\nTools = TOOLS\nfood = Food\nmisc = MISC\n"
         "a_very_long_key = LONG\n"
         "[item.a]\nname = A\ncat = food\n[item.b]\nname = B\ncat = misc\n");
    CHECK_EQ(it.cat_count, 1);
    CHECK_STR(it.cats[0].key, "misc");
    CHECK_EQ(it.item_count, 1);
    CHECK_STR(it.items[0].id, "b");
    /* Label of 6 letters, or digits: dropped. */
    load("version = 1\n[categories]\na = TOOLKT\nb = AB1\nc = ABCDE\n");
    CHECK_EQ(it.cat_count, 1);
    CHECK_STR(it.cats[0].label, "ABCDE");
    /* The 6th and later are ignored, with their items. */
    load("version = 1\n[categories]\na = A\nb = B\nc = C\nd = D\ne = E\nf = F\n"
         "[item.x]\nname = X\ncat = f\n[item.y]\nname = Y\ncat = e\n");
    CHECK_EQ(it.cat_count, 5);
    CHECK_EQ(it.item_count, 1);
    CHECK_STR(it.items[0].id, "y");
    /* A repeated key: the last label wins, the first place stays. */
    load("version = 1\n[categories]\na = A\nb = B\na = AA\n");
    CHECK_EQ(it.cat_count, 2);
    CHECK_STR(it.cats[0].label, "AA");
}

TEST(items_dropped)
{
    static const char *const bad[] = {
        "[item.Knife]\nname = K\ncat = tools\n",          /* bad id */
        "[item.a_very_long_id]\nname = K\ncat = tools\n", /* id of 14 */
        "[item.]\nname = K\ncat = tools\n",
        "[item.k]\ncat = tools\n",                        /* no name */
        "[item.k]\nname =\ncat = tools\n",                /* empty name */
        "[item.k]\nname = K\n",                           /* no cat */
        "[item.k]\nname = K\ncat = toys\n",               /* unknown cat */
        "[items.k]\nname = K\ncat = tools\n",             /* unknown section */
    };
    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        tfmt(text, sizeof(text), HEAD "%s", bad[i]);
        load(text);
        CHECK_EQ(it.load.status, DX_LOAD_OK);
        CHECK_EQ(it.item_count, 0);
    }
}

TEST(item_values)
{
    load(HEAD "[item.k]\nname = A folding knife with a long name\ncat = tools\n"
              "count = 1000\nunit = bundles\nweight = 1kg\n"
              "[item.l]\nname = L\ncat = tools\ncount = -1\nweight = 999999\n"
              "[item.m]\nname = M\ncat = tools\ncount = x\nnote2 = alone\n"
              "[item.n]\nname = N\ncat = tools\ncount = 0\nweight = 1234567\n");
    CHECK_EQ(it.item_count, 4);
    CHECK_STR(it.items[0].name, "A folding knife with a long na");
    CHECK_EQ(it.items[0].count, 999); /* clamped */
    CHECK_STR(it.items[0].unit, "bundle");
    CHECK_EQ(it.items[0].weight, DX_WEIGHT_NONE);
    CHECK_EQ(it.items[1].count, 1); /* a sign is not a number */
    CHECK_EQ(it.items[1].weight, 999999);
    CHECK_EQ(it.items[2].count, 1);
    CHECK_STR(it.items[2].note, ""); /* note2 without note: ignored */
    CHECK_EQ(it.items[3].count, 0);
    CHECK_EQ(it.items[3].weight, DX_WEIGHT_NONE);
}

TEST(note_cut_to_200)
{
    char n1[101], n2[101];

    memset(n1, 'x', 100);
    n1[100] = '\0';
    memset(n2, 'y', 100);
    n2[100] = '\0';
    tfmt(text, sizeof(text), HEAD "[item.a]\nname = A\ncat = food\nnote = %s\nnote2 = %s\n",
             n1, n2);
    load(text);
    CHECK_EQ((int)strlen(it.items[0].note), 200);
    CHECK_EQ(it.items[0].note[100], ' ');
    CHECK_EQ(it.items[0].note[199], 'y');
}

TEST(sections_merge_and_order)
{
    load(HEAD "[item.b]\nname = B\n[item.a]\nname = A\ncat = food\n[item.b]\ncat = tools\n"
              "name = B2\n");
    CHECK_EQ(it.item_count, 2);
    CHECK_STR(it.items[0].id, "b");
    CHECK_STR(it.items[0].name, "B2");
    CHECK_STR(it.items[1].id, "a");
    /* Items before [categories] still find their category. */
    load("version = 1\n[item.a]\nname = A\ncat = m\n[categories]\nm = MISC\n");
    CHECK_EQ(it.item_count, 1);
}

TEST(item_limit)
{
    int n = tfmt(text, sizeof(text), "version = 1\n[categories]\nm = MISC\n");
    int i;

    for (i = 0; i < 66; i++) {
        n += tfmt(text + n, sizeof(text) - (size_t)n, "[item.i%d]\nname = T\ncat = m\n", i);
    }
    load(text);
    CHECK_EQ(it.item_count, 64);
    CHECK_STR(it.items[63].id, "i63");
    /* A bad id does not take a slot. */
    n = tfmt(text, sizeof(text), "version = 1\n[categories]\nm = MISC\n[item.Bad]\nname = T\n");
    for (i = 0; i < 64; i++) {
        n += tfmt(text + n, sizeof(text) - (size_t)n, "[item.i%d]\nname = T\ncat = m\n", i);
    }
    load(text);
    CHECK_EQ(it.item_count, 64);
}

TEST(overlay)
{
    load(HEAD "[item.a]\nname = A\ncat = food\ncount = 5\n[item.b]\nname = B\ncat = food\n");
    CHECK_EQ(it.changes, 0);
    CHECK_EQ(dx_items_set_count(&it, 0, 5), 0); /* no change */
    CHECK_EQ(it.changes, 0);
    CHECK_EQ(dx_items_set_count(&it, 0, 1200), 1);
    CHECK_EQ(it.items[0].count, 999);
    CHECK_EQ(dx_items_add(&it, 0, 1), 0);
    CHECK_EQ(dx_items_add(&it, 0, -1000), 1);
    CHECK_EQ(it.items[0].count, 0);
    CHECK_EQ(dx_items_add(&it, 0, INT32_MIN), 0);
    CHECK_EQ(dx_items_add(&it, 0, INT32_MAX), 1);
    CHECK_EQ(it.items[0].count, 999);
    CHECK_EQ(dx_items_set_count(&it, 0, -5), 1);
    CHECK_EQ(it.items[0].count, 0);
    CHECK_EQ(dx_items_toggle_marked(&it, 1), 1);
    CHECK_EQ(it.items[1].marked, 1);
    CHECK_EQ(dx_items_toggle_marked(&it, 1), 1);
    CHECK_EQ(it.items[1].marked, 0);
    CHECK_EQ(it.changes, 6);
    CHECK_EQ(dx_items_set_count(&it, 2, 1), 0);
    CHECK_EQ(dx_items_set_count(&it, -1, 1), 0);
    CHECK_EQ(dx_items_toggle_marked(&it, 64), 0);
    CHECK_EQ(it.changes, 6);
    CHECK_EQ(dx_items_find(&it, "b"), 1);
    CHECK_EQ(dx_items_find(&it, "c"), -1);
}

TEST(totals)
{
    dx_cat_total_t t;

    load(HEAD "[item.a]\nname = A\ncat = food\nweight = 900\n[item.b]\nname = B\ncat = food\n"
              "[item.c]\nname = C\ncat = tools\nweight = 999999\ncount = 999\n");
    t = dx_items_total(&it, 1);
    CHECK_EQ(t.entries, 2);
    CHECK_EQ(t.grams, 900);
    t = dx_items_total(&it, 0);
    CHECK_EQ(t.entries, 1);
    CHECK_EQ(t.grams, 999999);
    t = dx_items_total(&it, 4);
    CHECK_EQ(t.entries, 0);
}

TEST(file_ignored)
{
    load("version = 2\n[categories]\nm = MISC\n");
    CHECK_EQ(it.load.status, DX_LOAD_VERSION);
    CHECK_EQ(it.cat_count, 0);
    tfmt(text, sizeof(text), HEAD "[item.a]\nname = A\ncat = food\nnote = %0130d\n", 0);
    load(text);
    CHECK_EQ(it.load.status, DX_LOAD_PARSE); /* a note above 127 is a parse error */
    CHECK_EQ(it.item_count, 0);
}

TEST(sample)
{
    static uint8_t buf[DX_ITEMS_LIMIT];
    int32_t n;
    int c;
    int sum = 0;

    fake_reset();
    CHECK(fake_load_tree("content", "") > 0);
    n = dx_plat_read_file("/PIPBOY/DATA/ITEMS.INI", buf, sizeof(buf));
    CHECK(n > 0);
    dx_items_load(&it, buf, n);
    CHECK_EQ(it.load.status, DX_LOAD_OK);
    CHECK_EQ(it.cat_count, 4);
    CHECK_EQ(it.item_count, 14);
    for (c = 0; c < it.cat_count; c++) {
        sum += dx_items_total(&it, c).entries;
    }
    CHECK_EQ(sum, 14);
    CHECK_STR(it.items[0].note, "A small knife with a wooden handle. Keep it dry and oil the "
                                "hinge once a month.");
}
