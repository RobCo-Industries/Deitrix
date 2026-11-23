/* The frame and the placeholder screen, drawn into the recorder. */
#include "rc_test.h"

#include <string.h>

#include "fake_platform.h"
#include "screens/screens.h"
#include "ui/frame.h"

static const char *const names[] = { "STAT", "INV", "DATA", "MAP", "RADIO" };

TEST(text_width)
{
    CHECK_EQ(dx_text_width("", 1), 0);
    CHECK_EQ(dx_text_width("STAT", 1), 32);
    CHECK_EQ(dx_text_width("RADIO", 3), 120);
}

static void check_inside(void)
{
    dx_rect_t screen = { 0, 0, DX_SCREEN_W, DX_SCREEN_H };

    CHECK(fake_points_inside(screen));
}

TEST(header_fits)
{
    dx_ctx_t c;
    int tab;

    for (tab = 0; tab < 5; tab++) {
        fake_reset();
        c = fake_ctx();
        dx_frame_header(&c, names, 5, tab, "23:59");
        check_inside();
    }
    /* The five tabs keep the skeleton's 8 px pad: the goldens stay. */
    CHECK_EQ(dx_frame_tab_pad(names, 5), 8);
    CHECK_EQ(dx_frame_tab_x(names, 5, 0), 12);
    CHECK(fake_find_text("23:59") != NULL);
    CHECK(dx_frame_tab_x(names, 5, 4) + dx_text_width("RADIO", 1) + 16 <
          fake_find_text("23:59")->x);
}

TEST(many_tabs_shrink_the_pad)
{
    static const char *const seven[] = { "LIST", "TEXT", "BAR", "KV", "TABS", "ICON", "FONT" };
    static const char *const eight[] = { "RADIO", "RADIO", "RADIO", "RADIO",
                                         "RADIO", "RADIO", "RADIO", "RADIO" };
    int pad = dx_frame_tab_pad(seven, 7);

    CHECK(pad >= 1 && pad < 8);
    /* The last tab still ends before the clock. */
    CHECK(dx_frame_tab_x(seven, 7, 6) + dx_text_width("FONT", 1) + pad <= 276 - 8);
    CHECK_EQ(dx_frame_tab_pad(eight, 8), 1);
}

TEST(active_tab_marked)
{
    dx_ctx_t c;
    int tab, i;

    for (tab = 0; tab < 5; tab++) {
        fake_reset();
        c = fake_ctx();
        dx_frame_header(&c, names, 5, tab, "--:--");
        /* One filled block, behind the active label only. */
        CHECK_EQ(fake.rect_count, 1);
        CHECK_EQ(fake.rects[0].filled, 1);
        CHECK_EQ(fake.rects[0].fg, fake.theme.fg);
        CHECK(fake.rects[0].x < dx_frame_tab_x(names, 5, tab));
        CHECK(fake.rects[0].x + fake.rects[0].w >
              dx_frame_tab_x(names, 5, tab) + dx_text_width(names[tab], 1));
        for (i = 0; i < 5; i++) {
            const fake_text_t *t = fake_find_text(names[i]);

            CHECK(t != NULL);
            if (t == NULL) {
                continue;
            }
            /* Dark on the block for the active tab, dim for the others. */
            CHECK_EQ(t->fg, i == tab ? 0u : fake.theme.dim);
        }
    }
}

TEST(header_draw_list)
{
    static const char *const two[] = { "AB", "CDE" };
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_frame_header(&c, two, 2, 1, "09:47");
    CHECK_STR(fake.draws, "text 12 3 \"AB\" dim none s1\n"
                          "rect 36 0 40 21 fg fill\n"
                          "text 44 3 \"CDE\" bg none s1\n"
                          "text 276 3 \"09:47\" fg none s1\n"
                          "line 0 21 319 21 fg\n");
}

TEST(footer_draw_list)
{
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_frame_footer(&c, DX_FRAME_HINT);
    CHECK_STR(fake.draws, "line 0 215 319 215 fg\n"
                          "text 40 220 \"DIAL or LEFT/RIGHT: change tab\" dim none s1\n");
}

TEST(placeholder_body)
{
    const fake_text_t *name, *note;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_placeholder_draw(&c, dx_frame_body(), "RADIO");
    CHECK(fake_points_inside(dx_frame_body()));
    name = fake_find_text("RADIO");
    note = fake_find_text("Not built yet.");
    CHECK(name != NULL && note != NULL);
    if (name == NULL || note == NULL) {
        return;
    }
    /* The skeleton's positions, so its goldens stay valid. */
    CHECK_EQ(name->scale, 3);
    CHECK_EQ(name->y, 72);
    CHECK_EQ(note->y, 144);
    CHECK_EQ(name->fg, fake.theme.fg);
    CHECK_EQ(note->fg, fake.theme.dim);
}

TEST(rom_screens)
{
    static const char *const want[] = { "STAT", "INV", "DATA", "MAP", "RADIO" };
    int i;

    CHECK_EQ(dx_rom_screen_count, 5);
    for (i = 0; i < 5 && i < dx_rom_screen_count; i++) {
        CHECK_STR(dx_rom_screens[i]->name, want[i]);
        CHECK(strlen(dx_rom_screens[i]->name) <= 5);
        CHECK(dx_rom_screens[i]->draw != NULL);
    }
}
