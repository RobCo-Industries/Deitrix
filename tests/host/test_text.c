/* Text wrapping and the scrolled text block. */
#include "rc_test.h"

#include <string.h>

#include "fake_platform.h"
#include "ui/text.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

/* Wraps and joins the lines with '|'. */
static const char *wrapped(const char *s, int cols)
{
    static char out[512];
    dx_line_t lines[32];
    int n = dx_wrap(s, cols, lines, 32);
    int i, o = 0;

    for (i = 0; i < n && i < 32; i++) {
        if (i > 0) {
            out[o++] = '|';
        }
        memcpy(out + o, s + lines[i].start, lines[i].len);
        o += lines[i].len;
    }
    out[o] = '\0';
    return out;
}

TEST(wrap_table)
{
    static const struct {
        const char *in;
        int cols;
        const char *out;
    } cases[] = {
        { "", 10, "" },
        { "short", 10, "short" },
        { "exactly10c", 10, "exactly10c" },
        { "the quick brown fox", 10, "the quick|brown fox" },
        { "the quick brown fox", 9, "the quick|brown fox" },
        { "the quick brown fox", 8, "the|quick|brown|fox" },
        { "aaaa bbbb", 4, "aaaa|bbbb" },
        { "aaaa  bbbb", 4, "aaaa|bbbb" },
        /* A word longer than a line is split. */
        { "abcdefghijkl", 5, "abcde|fghij|kl" },
        { "go abcdefghijkl", 5, "go|abcde|fghij|kl" },
        /* '\n' forces a break; an empty line stays. */
        { "one\ntwo", 10, "one|two" },
        { "one\n\ntwo", 10, "one||two" },
        { "one two\nthree four five", 10, "one two|three four|five" },
        /* Tabs count as spaces. */
        { "tab\there and there", 8, "tab\there|and|there" },
        /* Spaces at a wrap point are dropped. */
        { "ab   cd", 3, "ab|cd" },
        { "word ", 4, "word" },
        { "a b c d e f", 3, "a b|c d|e f" },
    };
    unsigned i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *got = wrapped(cases[i].in, cases[i].cols);

        if (strcmp(got, cases[i].out) != 0) {
            RC_FAIL("case %u: \"%s\" at %d: got \"%s\", want \"%s\"", i, cases[i].in,
                    cases[i].cols, got, cases[i].out);
        }
    }
}

TEST(wrap_never_exceeds_cols)
{
    static const char s[] = "Lorem-free sample: a garden needs water, light and some patience; "
                            "seedlings wilt\twhen\tthe soil dries out completely.\n"
                            "Supercalifragilisticexpialidocious words split.";
    dx_line_t lines[256];
    int cols;

    for (cols = 1; cols <= 40; cols++) {
        int n = dx_wrap(s, cols, lines, 256);
        int i;

        CHECK(n <= 256);
        for (i = 0; i < n && i < 256; i++) {
            CHECK(lines[i].len <= cols);
            CHECK(lines[i].len == 0 || s[lines[i].start] != ' ');
        }
    }
    CHECK_INT(dx_wrap(s, 0, lines, 64), 0);
}

TEST(wrap_counts_past_max)
{
    dx_line_t lines[2];

    CHECK_INT(dx_wrap("a b c d", 1, lines, 2), 4);
    CHECK_INT(lines[1].start, 2);
}

TEST(text_scroll)
{
    dx_text_t t;
    dx_ui_event_t up = { DX_UI_UP, 0, 0 }, down = { DX_UI_DOWN, 1, 0 };

    /* 104 px: 12 columns. Six lines, a window of three. */
    dx_text_init(&t, "one two three four five six seven eight nine ten", 104, 48);
    CHECK_INT(t.count, 5);
    CHECK_INT(t.visible, 3);
    CHECK_INT(dx_text_event(&t, &up), DX_UI_IGNORED);
    CHECK_INT(dx_text_event(&t, &down), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(dx_text_event(&t, &down), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(dx_text_event(&t, &down), DX_UI_IGNORED);
    CHECK_INT(t.top, 2);
    CHECK_INT(dx_text_event(&t, &up), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(t.top, 1);
}

TEST(draw_list_two_lines)
{
    dx_text_t t;
    dx_ctx_t c;

    fake_reset();
    c = fake_ctx();
    dx_text_init(&t, "Hello there,\tfriend", 120, 32);
    dx_text_draw(&t, &c, (dx_rect_t){ 0, 22, 120, 32 });
    CHECK_STR(fake.draws, "text 4 22 \"Hello there,\" fg none s1\n"
                          "text 4 38 \"friend\" fg none s1\n");
}

TEST(draw_inside_area_sweep)
{
    static const char s[] = "Every coordinate a text block draws must stay inside the area it "
                            "was given, whatever the width, the height and the scroll.";
    int w, h, k;

    for (w = 40; w <= 320; w += 28) {
        for (h = 16; h <= 192; h += 32) {
            dx_text_t t;
            dx_rect_t a = { 0, 22, w, h };
            dx_ui_event_t down = { DX_UI_DOWN, 0, 0 };

            dx_text_init(&t, s, w, h);
            for (k = 0; k < 30; k++) {
                dx_ctx_t c;

                fake_reset();
                c = fake_ctx();
                dx_text_draw(&t, &c, a);
                CHECK(fake_points_inside(a));
                dx_text_event(&t, &down);
            }
        }
    }
}
