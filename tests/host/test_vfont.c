/* The stroke font. */
#include "rc_test.h"

#include <string.h>

#include "fake_platform.h"
#include "ui/vfont.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static const char supported[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ :-./%";

/* Every glyph is well formed and its strokes stay in its cell. */
TEST(glyphs_inside_cell)
{
    const char *p;

    for (p = supported; *p != '\0'; p++) {
        const char *g = dx_vfont_glyph(*p);
        int pts = 0;

        CHECK(g != NULL);
        if (g == NULL) {
            continue;
        }
        while (*g != '\0') {
            if (*g == '*') {
                /* A filled square: two corners follow. */
                CHECK(pts == 0);
                g++;
                continue;
            }
            if (*g == '|') {
                /* A stroke has at least two points. */
                CHECK(pts >= 2);
                pts = 0;
                g++;
                continue;
            }
            CHECK(g[1] != '\0');
            if (g[1] == '\0') {
                break;
            }
            CHECK(g[0] >= '0' && g[0] - '0' <= DX_VFONT_W);
            CHECK(g[1] >= '0' && g[1] - '0' <= DX_VFONT_H);
            /* Inside the advance too, so glyphs never touch. */
            CHECK(g[0] - '0' < dx_vfont_advance(*p));
            pts++;
            g += 2;
        }
        CHECK(pts >= 2 || *p == ' ');
    }
}

TEST(glyphs_distinct)
{
    const char *a, *b;

    for (a = supported; *a != '\0'; a++) {
        for (b = a + 1; *b != '\0'; b++) {
            if (strcmp(dx_vfont_glyph(*a), dx_vfont_glyph(*b)) == 0) {
                RC_FAIL("'%c' and '%c' look the same", *a, *b);
            }
        }
    }
}

TEST(lowercase_and_unknown)
{
    CHECK_STR(dx_vfont_glyph('q'), dx_vfont_glyph('Q'));
    CHECK(dx_vfont_glyph('#') == NULL);
    CHECK(dx_vfont_glyph('\n') == NULL);
    CHECK(dx_vfont_glyph((char)0xC3) == NULL);
}

TEST(unknown_draws_a_box)
{
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    CHECK_INT(dx_vfont_draw(&c, 10, 20, "#", 2, fake.theme.fg), 12);
    CHECK_STR(fake.draws, "line 10 20 18 20 fg\n"
                          "line 18 20 18 32 fg\n"
                          "line 18 32 10 32 fg\n"
                          "line 10 32 10 20 fg\n"
                          "line 10 20 18 32 fg\n"
                          "line 18 20 10 32 fg\n");
}

TEST(width_is_sum_of_advances)
{
    int unit;

    for (unit = 1; unit <= 8; unit++) {
        int sum = 0;
        const char *p;

        for (p = supported; *p != '\0'; p++) {
            sum += dx_vfont_advance(*p) * unit;
        }
        CHECK_INT(dx_vfont_width(supported, unit), sum);
    }
    CHECK_INT(dx_vfont_width("12:45", 4), 4 * (6 + 6 + 3 + 6 + 6));
    CHECK_INT(dx_vfont_width("", 4), 0);
}

TEST(draw_list_small)
{
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    CHECK_INT(dx_vfont_draw(&c, 0, 30, "L-", 4, fake.theme.fg), 48);
    CHECK_STR(fake.draws, "line 0 30 0 54 fg\n"
                          "line 1 30 1 54 fg\n"
                          "line 0 31 0 55 fg\n"
                          "line 0 54 16 54 fg\n"
                          "line 1 54 17 54 fg\n"
                          "line 0 55 16 55 fg\n"
                          "line 24 42 40 42 fg\n"
                          "line 25 42 41 42 fg\n"
                          "line 24 43 40 43 fg\n");
    CHECK_INT(dx_vfont_weight(1), 1);
    CHECK_INT(dx_vfont_weight(2), 1);
    CHECK_INT(dx_vfont_weight(3), 2);
}

/* The dots of ':' and '.' are filled squares at every size, as wide as
 * the strokes. */
TEST(dots_are_filled)
{
    int unit;

    for (unit = 1; unit <= 8; unit++) {
        int wt = dx_vfont_weight(unit);
        dx_ctx_t c;

        fake_reset();
        c = fake_ctx();
        dx_vfont_draw(&c, 10, 20, ":.", unit, fake.theme.fg);
        CHECK_INT(fake.rect_count, 3);
        CHECK(fake.draws[0] == 'r' && strstr(fake.draws, "line") == NULL);
        if (fake.rect_count != 3) {
            continue;
        }
        CHECK_INT(fake.rects[0].x, 10);
        CHECK_INT(fake.rects[0].y, 20 + unit);
        CHECK_INT(fake.rects[0].w, unit + wt);
        CHECK_INT(fake.rects[0].h, unit + wt);
        CHECK_INT(fake.rects[1].y, 20 + 4 * unit);
        CHECK_INT(fake.rects[2].x, 10 + 3 * unit);
        CHECK_INT(fake.rects[2].y, 20 + 5 * unit);
        CHECK_INT(fake.rects[0].filled + fake.rects[1].filled + fake.rects[2].filled, 3);
    }
    fake_reset();
    {
        dx_ctx_t c = fake_ctx();

        dx_vfont_draw(&c, 0, 0, "1:2", 8, fake.theme.fg);
    }
    CHECK(strstr(fake.draws, "rect 48 8 10 10 fg fill\nrect 48 32 10 10 fg fill\n") != NULL);
}

/* Every line of the whole set stays in the string's box. */
TEST(draw_inside_box)
{
    int unit;

    for (unit = 1; unit <= 6; unit++) {
        dx_ctx_t c;
        int w;

        fake_reset();
        c = fake_ctx();
        w = dx_vfont_draw(&c, 5, 7, supported, unit, fake.theme.fg);
        CHECK(fake_points_inside(
            (dx_rect_t){ 5, 7, w, DX_VFONT_H * unit + dx_vfont_weight(unit) }));
    }
}
