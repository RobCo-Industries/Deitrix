/* The theme, event translation and the sound decision. */
#include "rc_test.h"

#include "fake_platform.h"
#include "ui/ui.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

TEST(theme_fractions)
{
    dx_theme_t t = dx_theme_from_fg(0x33e066u);

    CHECK_EQ(t.fg, 0x33e066u);
    CHECK_EQ(t.dim, 0x197033u);
    CHECK_EQ(t.faint, 0x0c3819u);
    CHECK_EQ(t.bg, 0u);
    t = dx_theme_from_fg(0xffb000u);
    CHECK_EQ(t.dim, 0x7f5800u);
    CHECK_EQ(t.faint, 0x3f2c00u);
    t = dx_theme_from_fg(0xffffffu);
    CHECK_EQ(t.dim, 0x7f7f7fu);
    CHECK_EQ(t.faint, 0x3f3f3fu);
    /* Bits above the colour are dropped. */
    CHECK_EQ(dx_theme_from_fg(0x01ffffffu).fg, 0xffffffu);
    /* No channel bleeds into the next. */
    CHECK_EQ(dx_theme_from_fg(0x010101u).dim, 0u);
}

static int tr(uint32_t type, uint32_t code, int32_t value, dx_ui_event_t *out)
{
    dx_event_t ev = { type, code, value };

    return dx_ui_translate(&ev, out);
}

TEST(translate_buttons)
{
    static const struct {
        uint32_t code;
        int type;
    } map[] = {
        { DX_BTN_UP, DX_UI_UP },         { DX_BTN_DOWN, DX_UI_DOWN },
        { DX_BTN_LEFT, DX_UI_LEFT },     { DX_BTN_RIGHT, DX_UI_RIGHT },
        { DX_BTN_SELECT, DX_UI_SELECT }, { DX_BTN_DIAL, DX_UI_SELECT },
    };
    dx_ui_event_t e;
    unsigned i;

    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        CHECK_INT(tr(DX_EV_BUTTON, map[i].code, DX_PRESS_DOWN, &e), 1);
        CHECK_INT(e.type, map[i].type);
        CHECK_INT(e.repeat, 0);
        CHECK_INT(tr(DX_EV_BUTTON, map[i].code, DX_PRESS_REPEAT, &e), 1);
        CHECK_INT(e.type, map[i].type);
        CHECK_INT(e.repeat, 1);
        CHECK_INT(tr(DX_EV_BUTTON, map[i].code, DX_PRESS_RELEASE, &e), 0);
    }
}

TEST(translate_dial_and_unknown)
{
    dx_ui_event_t e;

    CHECK_INT(tr(DX_EV_DIAL, 0, 3, &e), 1);
    CHECK_INT(e.type, DX_UI_DIAL);
    CHECK_INT(e.delta, 3);
    CHECK_INT(tr(DX_EV_DIAL, 0, -1, &e), 1);
    CHECK_INT(e.delta, -1);
    CHECK_INT(tr(DX_EV_DIAL, 0, 0, &e), 0);
    CHECK_INT(tr(DX_EV_BUTTON, 6, DX_PRESS_DOWN, &e), 0);
    CHECK_INT(tr(DX_EV_BUTTON, 0xffffffffu, DX_PRESS_DOWN, &e), 0);
    CHECK_INT(tr(DX_EV_BUTTON, DX_BTN_UP, 3, &e), 0);
    CHECK_INT(tr(7, DX_BTN_UP, DX_PRESS_DOWN, &e), 0);
    CHECK_INT(tr(DX_EV_NONE, 0, 0, &e), 0);
}

static int snd(int type, int repeat, unsigned result, int changed)
{
    dx_ui_event_t e = { type, repeat, 0 };

    return dx_ui_sound_for(&e, result, changed);
}

TEST(sound_for_outcome)
{
    const unsigned hd = DX_UI_HANDLED | DX_UI_DIRTY;

    CHECK_INT(snd(DX_UI_DIAL, 0, DX_UI_IGNORED, 1), DX_SND_MOVE);
    CHECK_INT(snd(DX_UI_LEFT, 0, DX_UI_IGNORED, 1), DX_SND_MOVE);
    CHECK_INT(snd(DX_UI_DIAL, 0, DX_UI_IGNORED, 0), DX_SND_NONE);
    CHECK_INT(snd(DX_UI_DOWN, 0, hd, 0), DX_SND_MOVE);
    CHECK_INT(snd(DX_UI_DOWN, 1, hd, 0), DX_SND_MOVE);
    CHECK_INT(snd(DX_UI_DOWN, 0, DX_UI_HANDLED, 0), DX_SND_NONE);
    CHECK_INT(snd(DX_UI_SELECT, 0, DX_UI_HANDLED, 0), DX_SND_SELECT);
    CHECK_INT(snd(DX_UI_BACK, 0, hd, 0), DX_SND_BACK);
    /* Ignored: DENY on a fresh press of a key that goes nowhere. */
    CHECK_INT(snd(DX_UI_DOWN, 0, DX_UI_IGNORED, 0), DX_SND_DENY);
    CHECK_INT(snd(DX_UI_UP, 0, DX_UI_IGNORED, 0), DX_SND_DENY);
    CHECK_INT(snd(DX_UI_SELECT, 0, DX_UI_IGNORED, 0), DX_SND_DENY);
    CHECK_INT(snd(DX_UI_BACK, 0, DX_UI_IGNORED, 0), DX_SND_DENY);
    CHECK_INT(snd(DX_UI_DOWN, 1, DX_UI_IGNORED, 0), DX_SND_NONE);
    CHECK_INT(snd(DX_UI_LEFT, 0, DX_UI_IGNORED, 0), DX_SND_NONE);
}

TEST(sound_plays_tone)
{
    fake_reset();
    dx_ui_sound(DX_SND_NONE);
    dx_ui_sound(99);
    CHECK_INT(fake.tone_count, 0);
    dx_ui_sound(DX_SND_MOVE);
    dx_ui_sound(DX_SND_DENY);
    CHECK_INT(fake.tone_count, 2);
    /* DENY is lower and longer than MOVE. */
    CHECK(fake.tones[1][0] < fake.tones[0][0]);
    CHECK(fake.tones[1][1] > fake.tones[0][1]);
}

TEST(itoa)
{
    char b[12];

    dx_itoa(0, b);
    CHECK_STR(b, "0");
    dx_itoa(120, b);
    CHECK_STR(b, "120");
    dx_itoa(-45, b);
    CHECK_STR(b, "-45");
    CHECK_INT(dx_itoa(INT32_MIN, b), 11);
    CHECK_STR(b, "-2147483648");
}

TEST(text_cut)
{
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    CHECK_INT(dx_text_cut(&c, 0, 0, "ABCDEF", 6, fake.theme.fg, DX_COLOR_NONE), 6);
    CHECK_INT(dx_text_cut(&c, 0, 0, "ABCDEFG", 6, fake.theme.fg, DX_COLOR_NONE), 6);
    CHECK_INT(dx_text_cut(&c, 0, 0, "AB", 0, fake.theme.fg, DX_COLOR_NONE), 0);
    CHECK_STR(fake.draws, "text 0 0 \"ABCDEF\" fg none s1\n"
                          "text 0 0 \"ABCDE~\" fg none s1\n");
}

TEST(is_back)
{
    static const int yes[] = { DX_UI_BACK, DX_UI_LEFT };
    static const int no[] = { DX_UI_UP, DX_UI_DOWN, DX_UI_RIGHT, DX_UI_SELECT, DX_UI_DIAL };
    unsigned i;
    int rep;

    for (rep = 0; rep <= 1; rep++) {
        for (i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) {
            dx_ui_event_t ev = { yes[i], rep, 0 };

            CHECK(dx_ui_is_back(&ev));
        }
        for (i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
            dx_ui_event_t ev = { no[i], rep, -1 };

            CHECK(!dx_ui_is_back(&ev));
        }
    }
}
