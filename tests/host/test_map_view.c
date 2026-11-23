/* The MAP tab's pure helpers: the pane fit, the lines in pixels, the
 * place cycling, the marker squares and the position text. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "fake_platform.h"
#include "model/map_view.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

/* The map pane of the sample layout: the body under the sub-tabs, less
 * the place row. */
#define PX 0
#define PY 42
#define PW 320
#define PH 154

static dx_map_t m;

static void box(int x0, int y0, int x1, int y1)
{
    memset(&m, 0, sizeof(m));
    m.box[0] = (int16_t)x0;
    m.box[1] = (int16_t)y0;
    m.box[2] = (int16_t)x1;
    m.box[3] = (int16_t)y1;
}

/* Every corner of the box lands inside the pane less the margin. */
static int corners_inside(int x, int y, int w, int h)
{
    dx_map_fit_t f = dx_map_pane_fit(&m, x, y, w, h);
    int cx[2] = { m.box[0], m.box[2] };
    int cy[2] = { m.box[1], m.box[3] };
    int i, j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            int px, py;

            dx_map_to_px(&f, cx[i], cy[j], &px, &py);
            if (px < x + DX_MAP_MARGIN || py < y + DX_MAP_MARGIN ||
                px > x + w - 1 - DX_MAP_MARGIN || py > y + h - 1 - DX_MAP_MARGIN) {
                return 0;
            }
        }
    }
    return 1;
}

TEST(pane_fit_sample_maps)
{
    dx_map_fit_t f;
    int px, py;

    /* The local sample: 400 by 300 into 304 by 138 inside the margin.
     * Height limits: 137 / 300 px per unit. */
    box(0, 0, 400, 300);
    f = dx_map_pane_fit(&m, PX, PY, PW, PH);
    CHECK_INT(f.num, 137);
    CHECK_INT(f.den, 300);
    CHECK_INT(f.oy, PY + DX_MAP_MARGIN);
    /* Centred across: 182 px of map in the 303 px span. */
    CHECK_INT(f.ox, PX + DX_MAP_MARGIN + (303 - 400 * 137 / 300) / 2);
    dx_map_to_px(&f, 400, 300, &px, &py);
    CHECK_INT(py, PY + PH - 1 - DX_MAP_MARGIN);
    /* Rounded to the nearest: 150 * 137 / 300 = 68.5 goes up. */
    dx_map_to_px(&f, 0, 150, &px, &py);
    CHECK_INT(py, PY + DX_MAP_MARGIN + 69);
    CHECK(corners_inside(PX, PY, PW, PH));
}

TEST(pane_fit_extremes)
{
    /* The whole range of the format, square, wide and tall. */
    box(0, 0, 999, 999);
    CHECK(corners_inside(PX, PY, PW, PH));
    box(0, 0, 999, 1);
    CHECK(corners_inside(PX, PY, PW, PH));
    box(0, 998, 999, 999);
    CHECK(corners_inside(PX, PY, PW, PH));
    box(0, 0, 1, 999);
    CHECK(corners_inside(PX, PY, PW, PH));
    box(998, 0, 999, 999);
    CHECK(corners_inside(PX, PY, PW, PH));
    /* The smallest box: 1 by 1 unit. */
    box(500, 500, 501, 501);
    CHECK(corners_inside(PX, PY, PW, PH));
    /* The locator of the detail view. */
    box(0, 0, 999, 1);
    CHECK(corners_inside(81, 117, 158, 93));
}

TEST(pane_too_small)
{
    dx_map_fit_t f;
    int px, py;

    /* No room inside the margin: every point at the corner, scale 0. */
    box(0, 0, 999, 999);
    f = dx_map_pane_fit(&m, 10, 20, 2 * DX_MAP_MARGIN, 50);
    CHECK_INT(f.num, 0);
    dx_map_to_px(&f, 999, 999, &px, &py);
    CHECK_INT(px, 10 + DX_MAP_MARGIN);
    CHECK_INT(py, 20 + DX_MAP_MARGIN);
}

TEST(line_px)
{
    dx_map_fit_t f;
    dx_polyline_t l;
    int16_t xy[2 * DX_POINTS_MAX];
    int i, n;

    box(0, 0, 999, 999);
    f = dx_map_pane_fit(&m, PX, PY, PW, PH);
    /* The most points, at both ends of the range. */
    l.count = DX_POINTS_MAX;
    for (i = 0; i < DX_POINTS_MAX; i++) {
        l.pts[i].x = (int16_t)(i % 2 ? 999 : 0);
        l.pts[i].y = (int16_t)(i / 2 % 2 ? 999 : 0);
    }
    n = dx_map_line_px(&f, &l, xy);
    CHECK_INT(n, DX_POINTS_MAX);
    for (i = 0; i < n; i++) {
        CHECK(xy[2 * i] >= PX + DX_MAP_MARGIN && xy[2 * i] <= PX + PW - 1 - DX_MAP_MARGIN);
        CHECK(xy[2 * i + 1] >= PY + DX_MAP_MARGIN &&
              xy[2 * i + 1] <= PY + PH - 1 - DX_MAP_MARGIN);
    }
    CHECK_INT(xy[0], f.ox);
    CHECK_INT(xy[1], f.oy);
    /* A line of one repeated point: both ends on one pixel. */
    l.count = 2;
    l.pts[0].x = l.pts[1].x = 700;
    l.pts[0].y = l.pts[1].y = 300;
    n = dx_map_line_px(&f, &l, xy);
    CHECK_INT(n, 2);
    CHECK_INT(xy[0], xy[2]);
    CHECK_INT(xy[1], xy[3]);
}

TEST(cycle)
{
    CHECK_INT(dx_map_cycle(0, 7, 1), 1);
    CHECK_INT(dx_map_cycle(6, 7, 1), 0);
    CHECK_INT(dx_map_cycle(0, 7, -1), 6);
    CHECK_INT(dx_map_cycle(3, 7, -1), 2);
    /* One place: cycling stays on it. */
    CHECK_INT(dx_map_cycle(0, 1, 1), 0);
    CHECK_INT(dx_map_cycle(0, 1, -1), 0);
    /* No place. */
    CHECK_INT(dx_map_cycle(-1, 0, 1), -1);
    CHECK_INT(dx_map_cycle(0, 0, -1), -1);
    /* A selection out of range starts again at the first. */
    CHECK_INT(dx_map_cycle(-1, 5, 1), 0);
    CHECK_INT(dx_map_cycle(9, 5, -1), 0);
    CHECK_INT(dx_map_cycle(DX_PLACES_MAX - 1, DX_PLACES_MAX, 1), 0);
}

TEST(squares)
{
    dx_map_square_t s = dx_map_square(100, 50, DX_MAP_MARK);

    CHECK_INT(s.x, 98);
    CHECK_INT(s.y, 48);
    CHECK_INT(s.size, 5);
    s = dx_map_square(100, 50, DX_MAP_MARK_SEL);
    CHECK_INT(s.x, 96);
    CHECK_INT(s.y, 46);
    /* The reticle stays inside the margin of a point on the fit's edge. */
    CHECK(DX_MAP_REACH < DX_MAP_MARGIN);
    CHECK(DX_MAP_MARK_SEL / 2 < DX_MAP_REACH);
}

TEST(pos_text)
{
    char buf[DX_MAP_POS_SIZE];
    dx_point_t p = { 280, 145 };

    CHECK_STR(dx_map_pos_text(p, buf), "280, 145");
    p.x = 0;
    p.y = 0;
    CHECK_STR(dx_map_pos_text(p, buf), "0, 0");
    p.x = DX_COORD_MAX;
    p.y = DX_COORD_MAX;
    CHECK_STR(dx_map_pos_text(p, buf), "999, 999");
}

TEST(zz_no_leaks)
{
    fake_reset();
    CHECK_INT(fake_live_blocks(), 0);
}
