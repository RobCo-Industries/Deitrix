/* The frame, drawn into the recorder. */
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
    int pad = dx_frame_tab_pad(seven, 7);

    /* The gallery's seven tabs keep their full names and a 4 px pad. */
    CHECK_EQ(dx_frame_tab_cut(seven, 7), 5);
    CHECK_EQ(pad, 4);
    CHECK(dx_frame_tab_x(seven, 7, 6) + dx_text_width("FONT", 1) + pad <= DX_TAB_RIGHT);
}

/* The labels the header drew, all left of the clock and on screen. */
static void check_labels(int count, int len, int active)
{
    int i;

    for (i = 0; i < fake.text_count; i++) {
        const fake_text_t *t = &fake.texts[i];
        int right = t->x + (int)strlen(t->text) * DX_FONT_W;

        if (t->x == DX_CLOCK_X) {
            continue;
        }
        if (right > DX_TAB_RIGHT || t->x < 0) {
            RC_FAIL("%d tabs of %d letters: \"%s\" spans %d to %d", count, len, t->text,
                    t->x, right);
        }
    }
    CHECK_EQ(fake.text_count, count + 1);
    /* The active block also ends before the clock. */
    CHECK_EQ(fake.rect_count, 1);
    CHECK(fake.rects[0].x >= 0 && fake.rects[0].x + fake.rects[0].w <= DX_TAB_RIGHT);
    (void)active;
}

TEST(tab_labels_never_reach_the_clock)
{
    static const char *const words[] = { "A", "AB", "ABC", "ABCD", "ABCDE" };
    const char *names[DX_MAX_TABS];
    int count, len, i;

    for (count = 1; count <= DX_MAX_TABS; count++) {
        for (len = 1; len <= DX_TAB_NAME_MAX; len++) {
            dx_ctx_t c;
            int cut;

            for (i = 0; i < count; i++) {
                names[i] = words[len - 1];
            }
            cut = dx_frame_tab_cut(names, count);
            CHECK(cut >= DX_TAB_CUT_MIN);
            CHECK(dx_frame_tab_pad(names, count) >= DX_TAB_MIN_PAD);
            fake_reset();
            c = fake_ctx();
            dx_frame_header(&c, names, count, count - 1, "23:59");
            check_labels(count, len, count - 1);
            check_inside();
        }
    }
}

TEST(tab_limit)
{
    static const char *const five[] = { "RADIO", "RADIO", "RADIO", "RADIO",
                                        "RADIO", "RADIO", "RADIO", "RADIO" };
    static const char *const four[] = { "DATA", "DATA", "DATA", "DATA",
                                        "DATA", "DATA", "DATA", "DATA" };

    /* Up to 6 tabs of 5 letters show in full, 7 show 4 letters, 8 show 3. */
    CHECK_EQ(dx_frame_tab_cut(five, 6), 5);
    CHECK_EQ(dx_frame_tab_cut(five, 7), 4);
    CHECK_EQ(dx_frame_tab_cut(five, 8), 3);
    /* 7 tabs of 4 letters show in full, 8 show 3. */
    CHECK_EQ(dx_frame_tab_cut(four, 7), 5);
    CHECK_EQ(dx_frame_tab_cut(four, 8), 3);
}

TEST(tab_cut_draw_list)
{
    static const char *const eight[] = { "RADIO", "STAT", "RADIO", "RADIO",
                                         "RADIO", "RADIO", "RADIO", "RADIO" };
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_frame_header(&c, eight, 8, 1, "--:--");
    CHECK_STR(fake.draws, "text 8 3 \"RAD\" dim none s1\n"
                          "rect 36 0 32 21 fg fill\n"
                          "text 40 3 \"STA\" bg none s1\n"
                          "text 72 3 \"RAD\" dim none s1\n"
                          "text 104 3 \"RAD\" dim none s1\n"
                          "text 136 3 \"RAD\" dim none s1\n"
                          "text 168 3 \"RAD\" dim none s1\n"
                          "text 200 3 \"RAD\" dim none s1\n"
                          "text 232 3 \"RAD\" dim none s1\n"
                          "text 276 3 \"--:--\" fg none s1\n"
                          "line 0 21 319 21 fg\n");
}

TEST(tab_table_refused)
{
    static const char *const nine[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I" };
    static const char *const long_name[] = { "STAT", "WEAPONS" };
    static const char *const empty[] = { "STAT", "" };
    static const char *const null_name[] = { "STAT", NULL };
    dx_ctx_t c;

    CHECK_EQ(dx_frame_tab_cut(nine, 9), 0);
    CHECK_EQ(dx_frame_tab_cut(nine, 0), 0);
    CHECK_EQ(dx_frame_tab_cut(long_name, 2), 0);
    CHECK_EQ(dx_frame_tab_cut(empty, 2), 0);
    CHECK_EQ(dx_frame_tab_cut(null_name, 2), 0);
    CHECK_EQ(dx_frame_tab_pad(long_name, 2), 0);
    /* A refused table draws the clock and the rule, no labels. */
    fake_reset();
    c = fake_ctx();
    dx_frame_header(&c, long_name, 2, 0, "--:--");
    CHECK_STR(fake.draws, "text 276 3 \"--:--\" fg none s1\n"
                          "line 0 21 319 21 fg\n");
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
                          "text 96 220 \"DIAL: change tab\" dim none s1\n");
}

TEST(rom_screens)
{
    static const char *const want[] = { "STAT", "INV", "DATA", "MAP", "RADIO" };
    int i;

    CHECK_EQ(dx_rom_screen_count, 5);
    for (i = 0; i < 5 && i < dx_rom_screen_count; i++) {
        CHECK_STR(dx_rom_screens[i]->name, want[i]);
        CHECK(strlen(dx_rom_screens[i]->name) <= DX_TAB_NAME_MAX);
        CHECK(dx_rom_screens[i]->draw != NULL);
    }
    /* The ROM's table draws in full. */
    {
        const char *n[DX_MAX_TABS];

        for (i = 0; i < dx_rom_screen_count; i++) {
            n[i] = dx_rom_screens[i]->name;
        }
        CHECK_EQ(dx_frame_tab_cut(n, dx_rom_screen_count), 5);
    }
}
