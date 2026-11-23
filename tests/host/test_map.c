/* MAP.INI and the fit of a box into pixels. */
#include "rc_test.h"

#include <stdio.h>
#include <string.h>

#include "fake_platform.h"
#include "model/map.h"

static dx_maps_t m;
static char text[4096];

static void load(const char *t)
{
    dx_map_load(&m, (const uint8_t *)t, (int32_t)strlen(t));
}

#define LOCAL "version = 1\n[map.local]\ntitle = T\nbox = 0,0,100,100\n"

TEST(maps)
{
    load("version = 1\n[map.world]\nbox = 0,0,9,9\n[map.local]\ntitle = Brook\nbox = 1,2,3,4\n");
    CHECK_EQ(m.load.status, DX_LOAD_OK);
    CHECK_EQ(m.count, 2);
    CHECK_EQ(m.maps[0].which, DX_MAP_LOCAL);
    CHECK_STR(m.maps[0].title, "Brook");
    CHECK_EQ(m.maps[0].box[3], 4);
    CHECK_EQ(m.maps[1].which, DX_MAP_WORLD);
    CHECK_STR(m.maps[1].title, "world"); /* the default title */
    load("version = 1\n[map.local]\ntitle = A title much longer than this\nbox = 0,0,1,1\n");
    CHECK_STR(m.maps[0].title, "A title much longer than");
}

TEST(map_dropped)
{
    static const char *const bad[] = {
        "[map.local]\ntitle = T\n",               /* no box */
        "[map.local]\nbox = 400,0,0,300\n",       /* x0 > x1 */
        "[map.local]\nbox = 0,0,0,300\n",         /* x0 == x1 */
        "[map.local]\nbox = 0,5,9,5\n",
        "[map.local]\nbox = 0,0,1000,600\n",      /* above 999 */
        "[map.local]\nbox = 0,0,10\n",
        "[map.local]\nbox = 0,0,10,10,10\n",
        "[map.local]\nbox = 0,0,x,10\n",
        "[map.local]\nbox = 0,0,10,10\nbox = 0,0,10\n", /* the last one wins */
        "[map.local.lines]\nl1 = 1,1 2,2\n",       /* no [map.local] */
        "[map.moon]\nbox = 0,0,9,9\n",
    };
    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        snprintf(text, sizeof(text), "version = 1\n%s", bad[i]);
        load(text);
        CHECK_EQ(m.load.status, DX_LOAD_OK);
        CHECK_EQ(m.count, 0);
    }
    load("version = 2\n[map.local]\nbox = 0,0,9,9\n");
    CHECK_EQ(m.load.status, DX_LOAD_VERSION);
    CHECK_EQ(m.count, 0);
}

TEST(lines)
{
    load(LOCAL "[map.local.lines]\n"
         "l3 = 0,0 100,100\n"
         "l1 = 1,1   2,2\t3,3\n"
         "l2 = 1,1\n"                 /* 1 point */
         "l4 = 1,1 200,2\n"           /* outside the box */
         "l5 = 1,1 2;2\n"             /* bad point */
         "l6 = 1,1 2,2,3\n"
         "l7 = 1,1 -2,2\n"
         "l65 = 1,1 2,2\n"            /* N above 64 */
         "l8 = 1,1 2,2 3,3 4,4 5,5 6,6 7,7 8,8 9,9 10,10 11,11 12,12 13,13 14,14 15,15 16,16\n"
         "l9 = 1,1 2,2 3,3 4,4 5,5 6,6 7,7 8,8 9,9 10,10 11,11 12,12 13,13 14,14 15,15 16,16 "
         "17,17\n");
    CHECK_EQ(m.count, 1);
    CHECK_EQ(m.maps[0].line_count, 3);
    CHECK_EQ(m.maps[0].lines[0].count, 3);
    CHECK_EQ(m.maps[0].lines[0].pts[2].x, 3);
    CHECK_EQ(m.maps[0].lines[1].count, 2);
    CHECK_EQ(m.maps[0].lines[1].pts[1].y, 100); /* edges included */
    CHECK_EQ(m.maps[0].lines[2].count, 16);
    /* Lines before the map's head are kept and checked against its box. */
    load("version = 1\n[map.local.lines]\nl1 = 1,1 2,2\nl2 = 1,1 50,50\n"
         "[map.local]\nbox = 0,0,10,10\n");
    CHECK_EQ(m.maps[0].line_count, 1);
}

TEST(places)
{
    load(LOCAL "[map.local.places]\n"
         "p2 = 5 , 6 , water , Spring, the old one\n"
         "p1 = 1,1,home,The house at the end of a lane\n"
         "p3 = 1,1,castle,Keep\n"
         "p4 = 1,101,home,House\n"
         "p5 = 1,1,home\n"
         "p6 = 1,1,home,  \n"
         "p7 = x,1,home,H\n"
         "p25 = 1,1,home,H\n"
         "p24 = 100,100,hazard,Edge\n");
    CHECK_EQ(m.maps[0].place_count, 3);
    CHECK_STR(m.maps[0].places[0].name, "The house at the end of ");
    CHECK_EQ(m.maps[0].places[0].kind, DX_PLACE_HOME);
    CHECK_EQ(m.maps[0].places[1].at.x, 5);
    CHECK_EQ(m.maps[0].places[1].at.y, 6);
    CHECK_EQ(m.maps[0].places[1].kind, DX_PLACE_WATER);
    CHECK_STR(m.maps[0].places[1].name, "Spring, the old one");
    CHECK_EQ(m.maps[0].places[2].kind, DX_PLACE_HAZARD);
    CHECK_STR(dx_place_kind_name(DX_PLACE_SHELTER), "shelter");
    CHECK_STR(dx_place_kind_name(9), "");
}

/* Every point of the box lands inside the rectangle; the corners land
 * on its edges along the limiting side. */
static void check_fit(int x0, int y0, int x1, int y1, int x, int y, int w, int h)
{
    int16_t box[4] = { (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1 };
    dx_map_fit_t f = dx_map_fit(box, x, y, w, h);
    int px, py, qx, qy;

    dx_map_to_px(&f, x0, y0, &px, &py);
    dx_map_to_px(&f, x1, y1, &qx, &qy);
    CHECK(px >= x && py >= y && qx <= x + w - 1 && qy <= y + h - 1);
    /* The limiting side is used in full, the other is centred. */
    CHECK((px == x && qx == x + w - 1) || (py == y && qy == y + h - 1));
    CHECK(px - x - (x + w - 1 - qx) <= 1 && (x + w - 1 - qx) - (px - x) <= 1);
    CHECK(py - y - (y + h - 1 - qy) <= 1 && (y + h - 1 - qy) - (py - y) <= 1);
}

TEST(fit_wide_tall_square)
{
    int16_t box[4] = { 0, 0, 400, 300 };
    dx_map_fit_t f;
    int px, py;

    /* 400x300 into 320x193: height limits, 192/300 px per unit. */
    f = dx_map_fit(box, 0, 22, 320, 193);
    CHECK_EQ(f.num, 192);
    CHECK_EQ(f.den, 300);
    dx_map_to_px(&f, 0, 0, &px, &py);
    CHECK_EQ(px, 31); /* (319 - 256) / 2 */
    CHECK_EQ(py, 22);
    dx_map_to_px(&f, 400, 300, &px, &py);
    CHECK_EQ(px, 31 + 256);
    CHECK_EQ(py, 22 + 192);
    check_fit(0, 0, 400, 300, 0, 22, 320, 193);
    check_fit(0, 0, 999, 10, 0, 22, 320, 193);  /* wide */
    check_fit(0, 0, 10, 999, 0, 22, 320, 193);  /* tall */
    check_fit(5, 5, 6, 6, 10, 10, 50, 50);      /* one unit */
    check_fit(0, 0, 999, 999, 3, 4, 1, 1);      /* one pixel */
    check_fit(100, 200, 300, 250, 7, 9, 120, 31);
}

TEST(fit_sweep)
{
    int bw, bh, w, h;

    for (bw = 1; bw <= 999; bw += 97) {
        for (bh = 1; bh <= 999; bh += 89) {
            for (w = 1; w <= 320; w += 53) {
                for (h = 1; h <= 193; h += 41) {
                    check_fit(0, 0, bw, bh, 0, 22, w, h);
                }
            }
        }
    }
}

TEST(fit_degenerate)
{
    int16_t flat[4] = { 5, 5, 5, 9 };
    int16_t good[4] = { 0, 0, 10, 10 };
    dx_map_fit_t f;
    int px, py;

    f = dx_map_fit(flat, 10, 20, 100, 100);
    CHECK_EQ(f.num, 0);
    dx_map_to_px(&f, 5, 9, &px, &py);
    CHECK_EQ(px, 10);
    CHECK_EQ(py, 20);
    f = dx_map_fit(good, 10, 20, 0, 100);
    CHECK_EQ(f.num, 0);
    f = dx_map_fit(good, 10, 20, 100, -4);
    dx_map_to_px(&f, 10, 10, &px, &py);
    CHECK_EQ(px, 10);
    CHECK_EQ(py, 20);
}

TEST(sample)
{
    static uint8_t buf[DX_MAP_LIMIT];
    int32_t n;

    fake_reset();
    CHECK(fake_load_tree("content", "") > 0);
    n = dx_plat_read_file("/PIPBOY/DATA/MAP.INI", buf, sizeof(buf));
    CHECK(n > 0);
    dx_map_load(&m, buf, n);
    CHECK_EQ(m.load.status, DX_LOAD_OK);
    CHECK_EQ(m.count, 2);
    CHECK_STR(m.maps[0].title, "Alder Brook");
    CHECK_EQ(m.maps[0].line_count, 11);
    CHECK_EQ(m.maps[0].place_count, 7);
    CHECK_EQ(m.maps[1].line_count, 5);
    CHECK_EQ(m.maps[1].place_count, 5);
}
