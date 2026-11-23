/* The frame and the placeholder screen, drawn into the fake platform. */
#include "rc_test.h"

#include "fake_platform.h"
#include "model/nav.h"
#include "screens/placeholder.h"
#include "ui/frame.h"

static const dx_theme_t green = { 0x33e066u, 0x197033u };

TEST(theme_dim_is_half)
{
    dx_theme_t t = dx_theme_from_fg(0x33e066u);

    CHECK_EQ(t.fg, 0x33e066u);
    CHECK_EQ(t.dim, 0x197033u);
    t = dx_theme_from_fg(0xffb000u);
    CHECK_EQ(t.dim, 0x7f5800u);
    /* Bits above the colour are dropped. */
    CHECK_EQ(dx_theme_from_fg(0x01ffffffu).fg, 0xffffffu);
}

TEST(text_width)
{
    CHECK_EQ(dx_text_width("", 1), 0);
    CHECK_EQ(dx_text_width("STAT", 1), 32);
    CHECK_EQ(dx_text_width("RADIO", 3), 120);
}

/* Every text call lies inside the screen and its band. */
static void check_inside(void)
{
    int i;

    for (i = 0; i < fake.text_count; i++) {
        const fake_text_t *t = &fake.texts[i];
        int32_t w = dx_text_width(t->text, t->scale);

        CHECK(t->x >= 0);
        CHECK(t->y >= 0);
        CHECK(t->x + w <= DX_SCREEN_W);
        CHECK(t->y + DX_FONT_H * (int32_t)t->scale <= DX_SCREEN_H);
    }
}

TEST(header_fits)
{
    int tab;

    /* The last tab ends before the clock starts, with a gap. */
    for (tab = 0; tab < DX_TAB_COUNT; tab++) {
        fake_reset();
        dx_frame_draw(&green, tab, "23:59");
        check_inside();
    }
    CHECK(fake_find_text("23:59") != NULL);
    CHECK(dx_frame_tab_x(DX_TAB_RADIO) + dx_text_width("RADIO", 1) + 16 <
          fake_find_text("23:59")->x);
}

TEST(active_tab_marked)
{
    int tab, i;

    for (tab = 0; tab < DX_TAB_COUNT; tab++) {
        fake_reset();
        dx_frame_draw(&green, tab, "--:--");
        /* One filled block, behind the active label only. */
        CHECK_EQ(fake.rect_count, 1);
        CHECK_EQ(fake.rects[0].filled, 1);
        CHECK_EQ(fake.rects[0].fg, green.fg);
        CHECK(fake.rects[0].x < dx_frame_tab_x(tab));
        CHECK(fake.rects[0].x + (int32_t)fake.rects[0].w >
              dx_frame_tab_x(tab) + dx_text_width(dx_tab_name(tab), 1));
        for (i = 0; i < DX_TAB_COUNT; i++) {
            const fake_text_t *t = fake_find_text(dx_tab_name(i));

            CHECK(t != NULL);
            if (t == NULL) {
                continue;
            }
            /* Dark on the block for the active tab, dim for the others. */
            CHECK_EQ(t->fg, i == tab ? 0u : green.dim);
        }
    }
}

TEST(clock_and_hint_drawn)
{
    fake_reset();
    dx_frame_draw(&green, 0, "--:--");
    CHECK(fake_find_text("--:--") != NULL);
    CHECK(fake_find_text("DIAL or LEFT/RIGHT: change tab") != NULL);
}

TEST(placeholder_body)
{
    const fake_text_t *name, *note;

    fake_reset();
    dx_placeholder_draw(&green, DX_TAB_RADIO);
    check_inside();
    name = fake_find_text("RADIO");
    note = fake_find_text("Not built yet.");
    CHECK(name != NULL && note != NULL);
    if (name == NULL || note == NULL) {
        return;
    }
    CHECK_EQ(name->scale, 3);
    CHECK_EQ(name->fg, green.fg);
    /* Below the header, above the footer, no overlap between them. */
    CHECK(name->y >= DX_BODY_TOP);
    CHECK(name->y + 3 * DX_FONT_H < note->y);
    CHECK(note->y + DX_FONT_H <= DX_BODY_BOTTOM);
}
