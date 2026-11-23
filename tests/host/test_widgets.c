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
    dx_gauge_draw(&g, &c, (dx_rect_t){ 0, 22, 200, 16 }, 6);
    CHECK_STR(fake.draws, "text 4 22 \"Power\" dim none s1\n"
                          "text 172 22 \"3/4\" fg none s1\n"
                          "rect 60 25 104 10 dim outline\n"
                          "rect 62 27 75 6 fg fill\n");
}

TEST(gauge_max_zero_draws_empty_bar)
{
    dx_gauge_t g;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_gauge_init(&g, "None", 7, 0);
    dx_gauge_draw(&g, &c, (dx_rect_t){ 0, 22, 200, 16 }, 6);
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
            dx_gauge_draw(&g, &c, a, 5);
            CHECK(fake_points_inside(a));
        }
    }
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
    CHECK_INT(sub_key(&s, DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(sub_key(&s, DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(sub_key(&s, DX_UI_RIGHT), DX_UI_IGNORED);
    CHECK_INT(s.active, 2);
    CHECK_INT(sub_key(&s, DX_UI_UP), DX_UI_IGNORED);
    CHECK_INT(sub_key(&s, DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY);
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
