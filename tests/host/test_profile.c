/* PROFILE.INI: one case per rule of docs/CONTENT.md, and the sample. */
#include "rc_test.h"

#include <string.h>

#include "fake_platform.h"
#include "model/profile.h"

static dx_profile_t p;

static void load(const char *text)
{
    dx_profile_load(&p, (const uint8_t *)text, (int32_t)strlen(text));
}

#define V "version = 1\n"

TEST(owner)
{
    load(V "[owner]\nname = Field kit 7\nline = A line\n");
    CHECK_EQ(p.load.status, DX_LOAD_OK);
    CHECK_STR(p.name, "Field kit 7");
    CHECK_STR(p.line, "A line");
    /* Longer: cut. Empty name: no name row. */
    load(V "[owner]\nname = A name that is far too long here\nline = "
           "0123456789012345678901234567890123456789\n");
    CHECK_STR(p.name, "A name that is far too l");
    CHECK_STR(p.line, "01234567890123456789012345678901234567");
    load(V "[owner]\nname =\n");
    CHECK_STR(p.name, "");
    /* A repeated key: the last one wins. */
    load(V "[owner]\nname = One\nname = Two\n");
    CHECK_STR(p.name, "Two");
}

TEST(gauges)
{
    load(V "[gauges]\ng2 = 3,4,Water\ng1 = 82,100,Condition\n");
    CHECK_EQ(p.gauge_count, 2);
    CHECK_STR(p.gauges[0].name, "Condition");
    CHECK_EQ(p.gauges[0].value, 82);
    CHECK_EQ(p.gauges[0].max, 100);
    CHECK_STR(p.gauges[1].name, "Water");
    /* value above max: clamped. Longer name: cut. Spaces around fields dropped. */
    load(V "[gauges]\ng1 = 9 , 5 , A gauge name that runs on\n");
    CHECK_EQ(p.gauge_count, 1);
    CHECK_EQ(p.gauges[0].value, 5);
    CHECK_STR(p.gauges[0].name, "A gauge name tha");
    /* The name is the last field and may hold commas. */
    load(V "[gauges]\ng1 = 1,2,Salt, pepper\n");
    CHECK_STR(p.gauges[0].name, "Salt, pepper");
}

TEST(gauges_dropped)
{
    static const char *const bad[] = {
        "g1 = 2,0,Food",      /* max 0 */
        "g1 = 2,x,Food",      /* max not a number */
        "g1 = 2,10000,Food",  /* max above 9999 */
        "g1 = x,3,Food",      /* value not a number */
        "g1 = -1,3,Food",     /* a sign */
        "g1 = 2,3,",          /* empty name */
        "g1 = 2,3,   ",       /* only spaces */
        "g1 = 2",             /* fields missing */
        "g1 = 2,3",
        "g1 = 1234567,9999,A", /* 7 digits */
        "g7 = 2,3,Food",      /* N above 6 */
        "g01 = 2,3,Food",     /* leading zero */
        "g0 = 2,3,Food",
        "G1 = 2,3,Food",      /* keys are case-sensitive */
    };
    char text[128];
    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        snprintf(text, sizeof(text), V "[gauges]\n%s\n", bad[i]);
        load(text);
        CHECK_EQ(p.load.status, DX_LOAD_OK);
        CHECK_EQ(p.gauge_count, 0);
    }
    /* The last one wins, even when it is bad. */
    load(V "[gauges]\ng1 = 1,2,Good\ng1 = 1,0,Bad\n");
    CHECK_EQ(p.gauge_count, 0);
}

TEST(attrs)
{
    load(V "[attrs]\na12 = 6,Last\na1 = 100,First\na3 = 7,Third name runs long\n"
           "a4 = x,Bad\na5 = 4,\na13 = 1,Over\n");
    CHECK_EQ(p.attr_count, 3);
    CHECK_STR(p.attrs[0].name, "First");
    CHECK_EQ(p.attrs[0].value, 99); /* clamped */
    CHECK_STR(p.attrs[1].name, "Third name runs ");
    CHECK_STR(p.attrs[2].name, "Last");
}

TEST(about)
{
    load(V "[about]\nl3 = Third\nl1 = First line that is much longer than thirty-eight\n"
           "l2 =\nl9 = No\n");
    CHECK_EQ(p.about_count, 3);
    CHECK_STR(p.about[0], "First line that is much longer than th");
    CHECK_STR(p.about[1], ""); /* a blank row */
    CHECK_STR(p.about[2], "Third");
}

TEST(sections_merge_and_unknown_ignored)
{
    load(V "[owner]\nname = A\n[stats]\nhp = 3\n[owner]\nline = B\nage = 4\n");
    CHECK_EQ(p.load.status, DX_LOAD_OK);
    CHECK_STR(p.name, "A");
    CHECK_STR(p.line, "B");
    /* Keys before the first section other than version: ignored. */
    load(V "color = 2\n[owner]\nname = A\n");
    CHECK_EQ(p.load.status, DX_LOAD_OK);
    /* A byte above 0x7E is kept: the screen draws it as a box. */
    load(V "[owner]\nname = Caf\xc3\xa9\n");
    CHECK_EQ(p.load.status, DX_LOAD_OK);
    CHECK_STR(p.name, "Caf\xc3\xa9");
}

TEST(file_ignored)
{
    load("[owner]\nname = A\n");
    CHECK_EQ(p.load.status, DX_LOAD_VERSION);
    CHECK_STR(p.name, "");
    load("version = 2\n[owner]\nname = A\n");
    CHECK_EQ(p.load.status, DX_LOAD_VERSION);
    load("version = 1\nversion = 2\n[owner]\nname = A\n"); /* the last one wins */
    CHECK_EQ(p.load.status, DX_LOAD_VERSION);
    load("[owner]\nversion = 1\n"); /* not before the first section */
    CHECK_EQ(p.load.status, DX_LOAD_VERSION);
    load(V "[owner]\nname = A\n!bad\n");
    CHECK_EQ(p.load.status, DX_LOAD_PARSE);
    CHECK_EQ(p.load.line, 4);
    CHECK_STR(p.name, ""); /* keys before the error are dropped too */
    load(V "[owner]\x01\n");
    CHECK_EQ(p.load.status, DX_LOAD_PARSE);
    dx_profile_load(&p, NULL, DX_FILE_MISSING);
    CHECK_EQ(p.load.status, DX_LOAD_MISSING);
    dx_profile_load(&p, NULL, DX_FILE_TOO_LARGE);
    CHECK_EQ(p.load.status, DX_LOAD_TOO_LARGE);
    dx_profile_load(&p, NULL, DX_FILE_ERROR);
    CHECK_EQ(p.load.status, DX_LOAD_READ_ERROR);
    load("");
    CHECK_EQ(p.load.status, DX_LOAD_VERSION);
}

TEST(crlf)
{
    load("version = 1\r\n[owner]\r\nname = A\r\n");
    CHECK_EQ(p.load.status, DX_LOAD_OK);
    CHECK_STR(p.name, "A");
}

TEST(sample)
{
    static uint8_t buf[DX_PROFILE_LIMIT];
    int32_t n;

    fake_reset();
    CHECK(fake_load_tree("content", "") > 0);
    n = dx_plat_read_file("/PIPBOY/DATA/PROFILE.INI", buf, sizeof(buf));
    CHECK(n > 0);
    dx_profile_load(&p, buf, n);
    CHECK_EQ(p.load.status, DX_LOAD_OK);
    CHECK_STR(p.name, "Field kit 7");
    CHECK_EQ(p.gauge_count, 4);
    CHECK_EQ(p.attr_count, 6);
    CHECK_EQ(p.about_count, 3);
}
