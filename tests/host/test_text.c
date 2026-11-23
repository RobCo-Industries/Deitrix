/* Text wrapping and the scrolled text block. */
#include "rc_test.h"
#include "tfmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_platform.h"
#include "ui/text.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

/* Wraps and joins the lines with '|'. */
static const char *wrapped(const char *s, int cols)
{
    static char out[512];
    dx_line_t lines[32];
    int n = dx_wrap(s, cols, lines, 32, NULL);
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
        int n = dx_wrap(s, cols, lines, 256, NULL);
        int i;

        CHECK(n <= 256);
        for (i = 0; i < n && i < 256; i++) {
            CHECK(lines[i].len <= cols);
            CHECK(lines[i].len == 0 || s[lines[i].start] != ' ');
        }
    }
    CHECK_INT(dx_wrap(s, 0, lines, 64, NULL), 0);
}

TEST(wrap_stops_at_max)
{
    dx_line_t lines[4];
    int cut = -1;

    CHECK_INT(dx_wrap("a b c d", 1, lines, 2, &cut), 2);
    CHECK_INT(lines[1].start, 2);
    CHECK_INT(cut, 1);
    /* Exactly max lines, trailing spaces included: not cut. */
    CHECK_INT(dx_wrap("a b c d  ", 1, lines, 4, &cut), 4);
    CHECK_INT(cut, 0);
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

/* ---- Size limits ---------------------------------------------------------- */

/* Draws the window scrolled to the bottom and returns the last text drawn. */
static const char *last_line(dx_text_t *t)
{
    static char out[48];
    dx_ctx_t c;
    dx_rect_t a = { 0, 22, 120, 32 };

    fake_reset();
    c = fake_ctx();
    t->top = t->count > t->visible ? t->count - t->visible : 0;
    dx_text_draw(t, &c, a);
    if (fake.text_count < 1) {
        return "";
    }
    tfmt(out, sizeof(out), "%s", fake.texts[fake.text_count - 1].text);
    return out;
}

/* "first\n", spaces up to `at`, then `tail`, in a buffer of `size`. */
static char *spaced(size_t size, size_t at, const char *tail)
{
    char *s = malloc(size + 1);
    size_t n = strlen(tail);

    memset(s, ' ', size);
    memcpy(s, "first\n", 6);
    memcpy(s + at, tail, n);
    s[at + n] = '\0';
    return s;
}

TEST(text_late_line_past_64k)
{
    /* The review's case: a long run of spaces, then a line past 65535. */
    char *s = spaced(70010, 70000, "late");
    dx_text_t t;

    dx_text_init(&t, s, 120, 32);
    CHECK_INT(t.count, 3);
    CHECK_INT(t.cut, 0);
    CHECK_STR(last_line(&t), "late");
    free(s);
}

TEST(text_bytes_just_under)
{
    char *s = spaced(DX_TEXT_MAX_BYTES, DX_TEXT_MAX_BYTES - 3, "end");
    dx_text_t t;

    CHECK_INT(strlen(s), DX_TEXT_MAX_BYTES);
    dx_text_init(&t, s, 120, 32);
    CHECK_INT(t.count, 3);
    CHECK_INT(t.cut, 0);
    CHECK_STR(last_line(&t), "end");
    free(s);
}

TEST(text_bytes_just_over)
{
    /* "late" crosses the limit: "la" is kept, then the mark. */
    char *s = spaced(DX_TEXT_MAX_BYTES + 2, DX_TEXT_MAX_BYTES - 2, "late");
    dx_text_t t;

    dx_text_init(&t, s, 120, 32);
    CHECK_INT(t.count, 4);
    CHECK_INT(t.cut, 1);
    CHECK_STR(last_line(&t), DX_TEXT_CUT_MARK);
    t.top = 1;
    fake_reset();
    {
        dx_ctx_t c = fake_ctx();

        dx_text_draw(&t, &c, (dx_rect_t){ 0, 22, 120, 32 });
    }
    CHECK_STR(fake.texts[0].text, "la");
    free(s);
}

/* Room for one "L<int>\n" line at any int value, plus the NUL. */
#define NUMBERED_LINE 14

/* n lines "L0" .. "L<n-1>". */
static char *numbered(int n)
{
    char *s = malloc((size_t)n * NUMBERED_LINE + 1);
    int i, o = 0;

    for (i = 0; i < n; i++) {
        o += tfmt(s + o, NUMBERED_LINE, i + 1 < n ? "L%d\n" : "L%d", i);
    }
    return s;
}

TEST(text_lines_just_under)
{
    char *s = numbered(DX_TEXT_MAX_LINES);
    dx_text_t t;

    dx_text_init(&t, s, 120, 32);
    CHECK_INT(t.count, DX_TEXT_MAX_LINES);
    CHECK_INT(t.cut, 0);
    CHECK_STR(last_line(&t), "L127");
    free(s);
}

TEST(text_lines_just_over)
{
    char *s = numbered(DX_TEXT_MAX_LINES + 1);
    dx_text_t t;

    dx_text_init(&t, s, 120, 32);
    CHECK_INT(t.count, DX_TEXT_MAX_LINES);
    CHECK_INT(t.cut, 1);
    CHECK_STR(last_line(&t), DX_TEXT_CUT_MARK);
    /* The line above the mark is the last one kept. */
    t.top = DX_TEXT_MAX_LINES - 2;
    fake_reset();
    {
        dx_ctx_t c = fake_ctx();

        dx_text_draw(&t, &c, (dx_rect_t){ 0, 22, 120, 32 });
    }
    CHECK_STR(fake.texts[0].text, "L126");
    free(s);
}
