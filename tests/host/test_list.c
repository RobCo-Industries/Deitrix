/* The list: selection, ends, the scroll window, cut labels, empty. */
#include "rc_test.h"

#include "fake_platform.h"
#include "ui/list.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static const dx_row_t rows3[] = {
    { "Lamp oil", "4", 0 },
    { "Spare fuse", NULL, DX_ROW_MARKED },
    { "Old map", "1", DX_ROW_DISABLED },
};

static void row3(void *user, int i, dx_row_t *out)
{
    (void)user;
    *out = rows3[i];
}

static void row_n(void *user, int i, dx_row_t *out)
{
    (void)user;
    (void)i;
    out->label = "Item";
    out->value = "99";
    out->flags = 0;
}

static unsigned key(dx_list_t *l, int type, int repeat)
{
    dx_ui_event_t ev = { type, repeat, 0 };

    return dx_list_event(l, &ev);
}

TEST(selection_and_ends)
{
    dx_list_t l;

    dx_list_init(&l, 3, 8, row3, NULL, "Empty");
    CHECK_INT(l.sel, 0);
    /* UP at the top: ignored, nothing moves. */
    CHECK_INT(key(&l, DX_UI_UP, 0), DX_UI_IGNORED);
    CHECK_INT(l.sel, 0);
    CHECK_INT(key(&l, DX_UI_DOWN, 0), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(key(&l, DX_UI_DOWN, 1), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(l.sel, 2);
    /* DOWN at the bottom: ignored, no wrap. */
    CHECK_INT(key(&l, DX_UI_DOWN, 1), DX_UI_IGNORED);
    CHECK_INT(l.sel, 2);
    CHECK_INT(key(&l, DX_UI_UP, 0), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(l.sel, 1);
    /* LEFT, RIGHT and SELECT are not the list's. */
    CHECK_INT(key(&l, DX_UI_LEFT, 0), DX_UI_IGNORED);
    CHECK_INT(key(&l, DX_UI_RIGHT, 0), DX_UI_IGNORED);
    CHECK_INT(key(&l, DX_UI_SELECT, 0), DX_UI_IGNORED);
    CHECK_INT(l.sel, 1);
}

TEST(empty_list_ignores_keys)
{
    dx_list_t l;

    dx_list_init(&l, 0, 8, row3, NULL, "Nothing here.");
    CHECK_INT(key(&l, DX_UI_DOWN, 0), DX_UI_IGNORED);
    CHECK_INT(key(&l, DX_UI_UP, 0), DX_UI_IGNORED);
    CHECK_INT(l.sel, 0);
}

TEST(window_examples)
{
    CHECK_INT(dx_list_window(0, 0, 12, 8), 0);
    CHECK_INT(dx_list_window(7, 0, 12, 8), 0);
    CHECK_INT(dx_list_window(8, 0, 12, 8), 1);
    CHECK_INT(dx_list_window(11, 1, 12, 8), 4);
    CHECK_INT(dx_list_window(4, 4, 12, 8), 4);
    CHECK_INT(dx_list_window(3, 4, 12, 8), 3);
    CHECK_INT(dx_list_window(5, 0, 5, 8), 0);
    /* A window past the end comes back. */
    CHECK_INT(dx_list_window(11, 9, 12, 8), 4);
}

/* Walks every size down and up: the selection stays in the window, the
 * window stays in the list, and it moves by at most one row a step. */
TEST(window_sweep)
{
    int count, visible;

    for (count = 0; count <= 30; count++) {
        for (visible = 1; visible <= 13; visible++) {
            dx_list_t l;
            int i, prev = 0;

            dx_list_init(&l, count, visible, row_n, NULL, NULL);
            for (i = 0; i < 2 * count + 2; i++) {
                key(&l, i < count + 1 ? DX_UI_DOWN : DX_UI_UP, 0);
                if (count > 0) {
                    CHECK(l.sel >= l.top && l.sel < l.top + visible);
                }
                CHECK(l.top >= 0);
                CHECK(l.top == 0 || l.top + visible <= count);
                CHECK(l.top - prev <= 1 && prev - l.top <= 1);
                prev = l.top;
            }
            CHECK_INT(l.sel, 0);
            CHECK_INT(l.top, 0);
        }
    }
}

static const dx_rect_t area3 = { 0, 22, 320, 48 };

TEST(draw_list_three_rows)
{
    dx_list_t l;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_list_init(&l, 3, 3, row3, NULL, NULL);
    key(&l, DX_UI_DOWN, 0);
    dx_list_draw(&l, &c, area3);
    CHECK_STR(fake.draws,
              "text 308 22 \"4\" fg none s1\n"
              "text 12 22 \"Lamp oil\" fg none s1\n"
              "rect 0 38 320 16 fg fill\n"
              "rect 4 44 4 4 bg fill\n"
              "text 12 38 \"Spare fuse\" bg fg s1\n"
              "text 308 54 \"1\" dim none s1\n"
              "text 12 54 \"Old map\" dim none s1\n");
}

TEST(draw_scroll_bar_when_rows_exceed)
{
    dx_list_t l;
    dx_ctx_t c;
    dx_rect_t a = { 0, 22, 320, 32 };

    fake_reset();
    c = fake_ctx();
    dx_list_init(&l, 3, 2, row3, NULL, NULL);
    dx_list_draw(&l, &c, a);
    /* A faint track and a thumb for 2 of 3 rows; rows end before it. */
    CHECK_STR(fake.draws,
              "line 318 22 318 53 faint\n"
              "rect 316 22 4 21 fg fill\n"
              "rect 0 22 314 16 fg fill\n"
              "text 302 22 \"4\" bg fg s1\n"
              "text 12 22 \"Lamp oil\" bg fg s1\n"
              "rect 4 44 4 4 fg fill\n"
              "text 12 38 \"Spare fuse\" fg none s1\n");
}

static void row_long(void *user, int i, dx_row_t *out)
{
    (void)user;
    out->label = "A very long label that would run into the value";
    out->value = i == 0 ? "12345" : NULL;
    out->flags = 0;
}

TEST(long_label_cut_before_value)
{
    dx_list_t l;
    dx_ctx_t c;
    const fake_text_t *v;
    int i;

    fake_reset();
    c = fake_ctx();
    dx_list_init(&l, 2, 2, row_long, NULL, NULL);
    dx_list_draw(&l, &c, area3);
    v = fake_find_text("12345");
    CHECK(v != NULL);
    for (i = 0; i < fake.text_count; i++) {
        const fake_text_t *t = &fake.texts[i];
        int len = 0;

        while (t->text[len] != '\0') {
            len++;
        }
        if (t == v) {
            continue;
        }
        CHECK(t->text[len - 1] == '~');
        if (v != NULL && t->y == v->y) {
            /* At least one cell between the label and the value. */
            CHECK(t->x + len * 8 + 8 <= v->x);
        }
        CHECK(t->x + len * 8 <= 320 - 4);
    }
}

TEST(empty_draws_centred_message)
{
    dx_list_t l;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_list_init(&l, 0, 8, row3, NULL, "No items");
    dx_list_draw(&l, &c, (dx_rect_t){ 0, 22, 320, 128 });
    CHECK_STR(fake.draws, "text 128 78 \"No items\" dim none s1\n");
}

/* Every coordinate stays in the area, for many sizes and selections. */
TEST(draw_inside_area_sweep)
{
    static const dx_rect_t areas[] = {
        { 0, 22, 320, 192 }, { 10, 40, 150, 64 }, { 200, 30, 100, 16 }, { 0, 22, 320, 50 },
    };
    unsigned a;
    int count, s;

    for (a = 0; a < sizeof(areas) / sizeof(areas[0]); a++) {
        for (count = 0; count <= 14; count++) {
            for (s = 0; s <= count; s++) {
                dx_list_t l;
                dx_ctx_t c;
                int k;

                fake_reset();
                c = fake_ctx();
                dx_list_init(&l, count, areas[a].h / 16, s % 2 ? row_long : row_n, NULL,
                             "Nothing to show here at all");
                for (k = 0; k < s; k++) {
                    key(&l, DX_UI_DOWN, 0);
                }
                dx_list_draw(&l, &c, areas[a]);
                if (!fake_points_inside(areas[a])) {
                    RC_FAIL("area %u count %d sel %d:\n%s", a, count, s, fake.draws);
                }
            }
        }
    }
}
