/* The gauge, key/value rows, sub-tabs and icons. */
#include "rc_test.h"

#include <string.h>

#include "fake_platform.h"
#include "ui/gauge.h"
#include "ui/icon.h"
#include "ui/kv.h"
#include "ui/subtabs.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

TEST(gauge_clamps)
{
    CHECK_INT(dx_gauge_clamp(5, 10), 5);
    CHECK_INT(dx_gauge_clamp(-3, 10), 0);
    CHECK_INT(dx_gauge_clamp(30, 10), 10);
    CHECK_INT(dx_gauge_clamp(5, 0), 0);
    CHECK_INT(dx_gauge_clamp(5, -1), 0);
    CHECK_INT(dx_gauge_fill(5, 10, 100), 50);
    CHECK_INT(dx_gauge_fill(30, 10, 100), 100);
    CHECK_INT(dx_gauge_fill(-1, 10, 100), 0);
    CHECK_INT(dx_gauge_fill(5, 0, 100), 0);
    CHECK_INT(dx_gauge_fill(INT32_MAX, INT32_MAX, 300), 300);
    CHECK_INT(dx_gauge_fill(1, 3, 100), 33);
}

TEST(gauge_draw_list)
{
    dx_gauge_t g;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_gauge_init(&g, "Power", 3, 4);
    dx_gauge_draw(&g, &c, (dx_rect_t){ 0, 22, 200, 16 }, 6, 3);
    CHECK_STR(fake.draws, "text 4 22 \"Power\" dim none s1\n"
                          "text 172 22 \"3/4\" fg none s1\n"
                          "rect 60 25 104 10 dim outline\n"
                          "rect 62 27 75 6 fg fill\n");
}

TEST(gauge_selected_value_only_and_pad)
{
    dx_gauge_t g;
    dx_ctx_t c;
    char buf[24];

    fake_reset();
    c = fake_ctx();
    dx_gauge_init(&g, "Power", 3, 4);
    CHECK_INT(g.selected, 0);
    CHECK_INT(g.value_only, 0);
    CHECK_INT(g.pad, 4);
    g.selected = 1;
    g.value_only = 1;
    g.pad = 12;
    CHECK_INT(dx_gauge_text(&g, buf), 1);
    CHECK_STR(buf, "3");
    dx_gauge_draw(&g, &c, (dx_rect_t){ 0, 22, 200, 16 }, 5, 3);
    /* Inverse: the row in fg, everything else in bg on it. */
    CHECK_STR(fake.draws, "rect 0 22 200 16 fg fill\n"
                          "text 12 22 \"Power\" bg fg s1\n"
                          "text 188 22 \"3\" bg fg s1\n"
                          "rect 60 25 104 10 bg outline\n"
                          "rect 62 27 75 6 bg fill\n");
}

TEST(gauge_max_zero_draws_empty_bar)
{
    dx_gauge_t g;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_gauge_init(&g, "None", 7, 0);
    dx_gauge_draw(&g, &c, (dx_rect_t){ 0, 22, 200, 16 }, 6, 3);
    CHECK(fake_find_text("0/0") != NULL);
    CHECK_INT(fake.rect_count, 1);
    CHECK_INT(fake.rects[0].filled, 0);
}

TEST(gauge_inside_area_sweep)
{
    int32_t v;
    int w;

    for (w = 60; w <= 320; w += 20) {
        for (v = -5; v <= 130; v += 15) {
            dx_gauge_t g;
            dx_ctx_t c;
            dx_rect_t a = { 0, 50, w, 16 };

            fake_reset();
            c = fake_ctx();
            dx_gauge_init(&g, "Label", v, 120);
            dx_gauge_draw(&g, &c, a, 5, 7);
            CHECK(fake_points_inside(a));
        }
    }
}

/* A group shares value_cols: every bar has the same rectangle, and every
 * number ends at the same x. */
TEST(gauge_group_aligned)
{
    static const int32_t vals[][2] = { { 72, 100 }, { 3, 5 }, { 130, 120 }, { 4, 0 }, { 0, 7 } };
    dx_gauge_t gs[5];
    int cols, i;

    for (i = 0; i < 5; i++) {
        dx_gauge_init(&gs[i], "Label", vals[i][0], vals[i][1]);
    }
    cols = dx_gauge_value_cols(gs, 5);
    CHECK_INT(cols, 7); /* "120/120" */
    for (i = 0; i < 5; i++) {
        dx_ctx_t c;
        char buf[24];
        int n = dx_gauge_text(&gs[i], buf);

        fake_reset();
        c = fake_ctx();
        dx_gauge_draw(&gs[i], &c, (dx_rect_t){ 0, 40, 300, 16 }, 8, cols);
        CHECK(fake.rect_count >= 1);
        CHECK_INT(fake.rects[0].x, 76);
        CHECK_INT(fake.rects[0].y, 43);
        CHECK_INT(fake.rects[0].w, 156);
        CHECK_INT(fake.rects[0].h, 10);
        CHECK(fake_find_text(buf) != NULL);
        if (fake_find_text(buf) != NULL) {
            CHECK_INT(fake_find_text(buf)->x + n * 8, 296);
        }
    }
}

TEST(gauge_text_wider_than_column)
{
    dx_gauge_t g;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_gauge_init(&g, "Big", 1000, 10000);
    dx_gauge_draw(&g, &c, (dx_rect_t){ 0, 22, 200, 16 }, 4, 3);
    /* "1000/10000" keeps its full width and the bar gives way. */
    CHECK_STR(fake.draws, "text 4 22 \"Big\" dim none s1\n"
                          "text 116 22 \"1000/10000\" fg none s1\n"
                          "rect 44 25 64 10 dim outline\n"
                          "rect 46 27 6 6 fg fill\n");
}

static const dx_kv_row_t kv_rows[] = {
    { "Unit", "Model 3" },
    { "Firmware", "0.1.0" },
};

TEST(kv_draw_list)
{
    dx_kv_t kv;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_kv_init(&kv, kv_rows, 2);
    CHECK_INT(kv.key_cols, 8);
    dx_kv_draw(&kv, &c, (dx_rect_t){ 0, 22, 320, 32 });
    CHECK_STR(fake.draws, "text 4 22 \"Unit\" dim none s1\n"
                          "text 84 22 \"Model 3\" fg none s1\n"
                          "text 4 38 \"Firmware\" dim none s1\n"
                          "text 84 38 \"0.1.0\" fg none s1\n");
}

TEST(kv_rows_that_do_not_fit_are_left_out)
{
    dx_kv_t kv;
    dx_ctx_t c;
    dx_rect_t a = { 20, 40, 100, 20 };

    fake_reset();
    c = fake_ctx();
    dx_kv_init(&kv, kv_rows, 2);
    dx_kv_draw(&kv, &c, a);
    CHECK_INT(fake.text_count, 2);
    CHECK(fake_points_inside(a));
}

static const char *const labels[] = { "ALL", "TOOLS", "FOOD" };

static unsigned sub_key(dx_subtabs_t *s, int type)
{
    dx_ui_event_t ev = { type, 0, 0 };

    return dx_subtabs_event(s, &ev);
}

TEST(subtabs_ends)
{
    dx_subtabs_t s;

    dx_subtabs_init(&s, labels, 3);
    CHECK_INT(sub_key(&s, DX_UI_LEFT), DX_UI_IGNORED);
    CHECK_INT(s.active, 0);
    CHECK_INT(sub_key(&s, DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_TAB_MOVED);
    CHECK_INT(sub_key(&s, DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_TAB_MOVED);
    CHECK_INT(sub_key(&s, DX_UI_RIGHT), DX_UI_IGNORED);
    CHECK_INT(s.active, 2);
    CHECK_INT(sub_key(&s, DX_UI_UP), DX_UI_IGNORED);
    CHECK_INT(sub_key(&s, DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_TAB_MOVED);
    CHECK_INT(s.active, 1);
    /* Six labels: the sixth is dropped. */
    {
        static const char *const six[] = { "A", "B", "C", "D", "E", "F" };

        dx_subtabs_init(&s, six, 6);
        CHECK_INT(s.count, 5);
    }
}

TEST(subtabs_draw_list)
{
    dx_subtabs_t s;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_subtabs_init(&s, labels, 3);
    sub_key(&s, DX_UI_RIGHT);
    dx_subtabs_draw(&s, &c, (dx_rect_t){ 0, 22, 320, 40 });
    CHECK_STR(fake.draws, "text 4 23 \"ALL\" dim none s1\n"
                          "text 44 23 \"TOOLS\" fg none s1\n"
                          "rect 44 39 40 2 fg fill\n"
                          "text 100 23 \"FOOD\" dim none s1\n"
                          "line 0 41 319 41 faint\n");
    CHECK(fake_points_inside((dx_rect_t){ 0, 22, 320, DX_SUBTABS_H }));
}

TEST(icons_generated)
{
    const dx_icon_t *note = dx_icon_find("note");
    dx_ctx_t c;
    int i;

    CHECK_INT(dx_icon_count, 3);
    CHECK(note != NULL);
    CHECK(dx_icon_find("speaker") != NULL);
    CHECK(dx_icon_find("pin") != NULL);
    CHECK(dx_icon_find("nope") == NULL);
    CHECK(dx_icon_find("not") == NULL);
    for (i = 0; i < dx_icon_count; i++) {
        CHECK_INT(dx_icons[i].stride, (dx_icons[i].w + 7) / 8);
        CHECK(i == 0 || strcmp(dx_icons[i - 1].name, dx_icons[i].name) < 0);
    }
    if (note == NULL) {
        return;
    }
    fake_reset();
    c = fake_ctx();
    dx_icon_draw(note, &c, (dx_rect_t){ 10, 30, 40, 40 }, fake.theme.fg);
    CHECK_STR(fake.draws, "blit1 18 38 24 24 stride 3 fg none\n");
    /* Too big for the area: nothing. */
    fake_clear_draws();
    dx_icon_draw(note, &c, (dx_rect_t){ 10, 30, 20, 40 }, fake.theme.fg);
    CHECK_STR(fake.draws, "");
}
