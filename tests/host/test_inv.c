/* The INV tab against the fake platform: every view, key and limit. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "content.h"
#include "fake_platform.h"
#include "screens/inv_screen.h"
#include "screens/screens.h"
#include "ui/frame.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

#define ITEMS_PATH "/PIPBOY/DATA/ITEMS.INI"
#define TAB_INV    1

static const dx_screen_t *const inv = &dx_screen_inv;
static char text[DX_ITEMS_LIMIT];

static const char SMALL[] =
    "version = 1\n[categories]\ntools = TOOLS\nfood = FOOD\nempty = EMPTY\n"
    "[item.knife]\nname = Folding knife\ncat = tools\ncount = 1\nweight = 90\n"
    "note = A small knife with a wooden handle.\n"
    "[item.rope]\nname = Rope\ncat = tools\ncount = 20\nunit = m\nweight = 1400\n"
    "[item.bread]\nname = Rye bread\ncat = food\ncount = 0\nunit = loaf\n";

/* Loads `items` (NULL: no file) as the card's ITEMS.INI. */
static void load(const char *items)
{
    fake_reset();
    if (items != NULL) {
        fake_add_text(ITEMS_PATH, items);
    }
    dx_content_load();
    dx_inv_screen_reset();
    fake.log[0] = '\0';
}

static unsigned send(int type, int repeat)
{
    dx_ui_event_t ev = { type, repeat, 0 };

    return inv->event(inv->st, &ev);
}

static unsigned key(int type)
{
    return send(type, 0);
}

/* Draws the body alone into a fresh draw list. */
static void draw(void)
{
    dx_ctx_t c = fake_ctx();

    fake_clear_draws();
    inv->draw(inv->st, &c, dx_frame_body());
}

static int has_text(const char *t)
{
    return fake_find_text(t) != NULL;
}

/* The last log line, without its line end. */
static const char *last_log(void)
{
    static char line[128];
    size_t n = strlen(fake.log);
    size_t start;

    if (n == 0) {
        return "";
    }
    start = n - 1;
    while (start > 0 && fake.log[start - 1] != '\n') {
        start--;
    }
    tfmt(line, sizeof(line), "%.*s", (int)(n - 1 - start), fake.log + start);
    return line;
}

/* Every point inside the body, and every text inside it too. */
static int inside_body(void)
{
    dx_rect_t b = dx_frame_body();
    int i;

    for (i = 0; i < fake.text_count; i++) {
        const fake_text_t *t = &fake.texts[i];

        if (t->x < b.x || t->x + (int32_t)strlen(t->text) * DX_FONT_W > b.x + b.w ||
            t->y < b.y || t->y + DX_FONT_H > b.y + b.h) {
            return 0;
        }
    }
    return fake_points_inside(b);
}

TEST(no_file)
{
    load(NULL);
    draw();
    CHECK(has_text("No items on the card."));
    CHECK(has_text("ITEMS.INI missing"));
    CHECK(inside_body());
    CHECK(inv->hint(inv->st) == NULL);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_DOWN), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: inv idle");
    /* LEFT and RIGHT that change the tab log nothing here. */
    fake.log[0] = '\0';
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_IGNORED);
    CHECK_STR(fake.log, "");
}

TEST(broken_file_and_no_categories)
{
    load("version = 1\n[categories]\nbad line\n");
    draw();
    CHECK(has_text("No items on the card."));
    CHECK(has_text("ITEMS.INI line 3"));
    load("version = 1\n[categories]\n");
    draw();
    CHECK(has_text("No items on the card."));
    CHECK(fake.text_count == 1);
    CHECK(inv->hint(inv->st) == NULL);
}

TEST(first_view_draw_list)
{
    load(SMALL);
    draw();
    CHECK_STR(fake.draws,
              "text 4 23 \"TOOLS\" fg none s1\n"
              "rect 4 39 40 2 fg fill\n"
              "text 60 23 \"FOOD\" dim none s1\n"
              "text 108 23 \"EMPTY\" dim none s1\n"
              "line 0 41 319 41 faint\n"
              "rect 0 48 320 16 fg fill\n"
              "text 308 48 \"1\" bg fg s1\n"
              "text 12 48 \"Folding knife\" bg fg s1\n"
              "text 284 64 \"20 m\" fg none s1\n"
              "text 12 64 \"Rope\" fg none s1\n"
              "line 0 193 319 193 faint\n"
              "text 12 197 \"2 entries, 1.5 kg\" dim none s1\n");
    CHECK(inside_body());
    CHECK_STR(inv->hint(inv->st), "UP/DOWN: move  SELECT: open");
}

TEST(categories_and_empty_category)
{
    load(SMALL);
    CHECK_INT(key(DX_UI_LEFT), DX_UI_IGNORED);
    CHECK_STR(fake.log, "");
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: inv cat=1 row=0");
    draw();
    /* A count of 0 is dim. No weight in the category: entries only. */
    CHECK_STR(strstr(fake.draws, "rect 0 48"),
              "rect 0 48 320 16 dim fill\n"
              "text 268 48 \"0 loaf\" bg dim s1\n"
              "text 12 48 \"Rye bread\" bg dim s1\n"
              "line 0 193 319 193 faint\n"
              "text 12 197 \"1 entry\" dim none s1\n");
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    draw();
    CHECK(has_text("Nothing here."));
    CHECK(!has_text("0 entries"));
    CHECK(inside_body());
    CHECK_STR(inv->hint(inv->st), "LEFT/RIGHT: page");
    /* Nothing to open, nothing to move. */
    CHECK_INT(key(DX_UI_SELECT), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_DOWN), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_BACK), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: inv idle");
    CHECK_STR(fake.log, "deitrix: inv cat=1 row=0\ndeitrix: inv cat=2 row=0\n"
                        "deitrix: inv idle\ndeitrix: inv end row=0\ndeitrix: inv idle\n");
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: inv cat=0 row=0");
}

TEST(list_moves_and_ends)
{
    load(SMALL);
    CHECK_INT(key(DX_UI_UP), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: inv end row=0");
    CHECK_INT(key(DX_UI_DOWN), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_ROW_MOVED);
    CHECK_STR(last_log(), "deitrix: inv cat=0 row=1");
    CHECK_INT(send(DX_UI_DOWN, 1), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: inv end row=1");
    CHECK_INT(key(DX_UI_BACK), DX_UI_IGNORED);
    /* Each category keeps its row. */
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: inv cat=0 row=1");
    draw();
    CHECK(strstr(fake.draws, "text 12 64 \"Rope\" bg fg s1\n") != NULL);
}

TEST(detail_view)
{
    load(SMALL);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK_STR(last_log(), "deitrix: inv open=knife");
    draw();
    CHECK_STR(fake.draws,
              "text 4 23 \"Folding knife\" fg none s1\n"
              "line 0 41 319 41 faint\n"
              "text 4 48 \"Count\" dim none s1\n"
              "text 84 48 \"1\" fg none s1\n"
              "text 4 64 \"Weight\" dim none s1\n"
              "text 84 64 \"90 g\" fg none s1\n"
              "text 4 80 \"Category\" dim none s1\n"
              "text 84 80 \"TOOLS\" fg none s1\n"
              "text 4 96 \"Marked\" dim none s1\n"
              "text 84 96 \"No\" fg none s1\n"
              "text 4 120 \"A small knife with a wooden handle.\" dim none s1\n");
    CHECK(inside_body());
    CHECK_STR(inv->hint(inv->st), "UP/DOWN: +/-  SELECT: mark  BACK: list");
    /* An item without weight or note: no Weight row, no note. */
    CHECK_INT(key(DX_UI_BACK), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    draw();
    CHECK(!has_text("Weight"));
    CHECK(has_text("0 loaf"));
    CHECK(fake.text_count == 7);
}

TEST(count_to_both_limits)
{
    dx_items_t *it = dx_content_items();
    uint32_t changes;
    int i;

    load(SMALL);
    key(DX_UI_SELECT);
    changes = it->changes;
    CHECK_INT(key(DX_UI_DOWN), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: inv count=0");
    CHECK_INT(it->changes, changes + 1);
    /* At 0: refused, press and repeat, and no change counted. */
    CHECK_INT(key(DX_UI_DOWN), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: inv refused count=0");
    CHECK_INT(send(DX_UI_DOWN, 1), DX_UI_IGNORED);
    CHECK_INT(it->changes, changes + 1);
    /* A press then repeats: one step per event, up to 999. */
    CHECK_INT(key(DX_UI_UP), DX_UI_HANDLED | DX_UI_DIRTY);
    for (i = 2; i <= 999; i++) {
        fake.log[0] = '\0';
        CHECK_INT(send(DX_UI_UP, 1), DX_UI_HANDLED | DX_UI_DIRTY);
        CHECK_INT(it->items[0].count, i);
    }
    CHECK_STR(last_log(), "deitrix: inv count=999");
    CHECK_INT(it->changes, changes + 1000);
    CHECK_INT(send(DX_UI_UP, 1), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_UP), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: inv refused count=999");
    CHECK_INT(it->changes, changes + 1000);
    for (i = 998; i >= 0; i--) {
        fake.log[0] = '\0';
        CHECK_INT(send(DX_UI_DOWN, 1), DX_UI_HANDLED | DX_UI_DIRTY);
        CHECK_INT(it->items[0].count, i);
    }
    CHECK_INT(send(DX_UI_DOWN, 1), DX_UI_IGNORED);
    CHECK_INT(it->items[0].count, 0);
}

TEST(mark_back_and_summary)
{
    dx_items_t *it = dx_content_items();
    uint32_t changes;

    load(SMALL);
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: inv open=rope");
    changes = it->changes;
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: inv marked=1");
    CHECK_INT(it->changes, changes + 1);
    /* A held SELECT does not toggle again. */
    CHECK_INT(send(DX_UI_SELECT, 1), DX_UI_IGNORED);
    CHECK_INT(it->items[1].marked, 1);
    draw();
    CHECK(has_text("Yes"));
    CHECK_INT(key(DX_UI_UP), DX_UI_HANDLED | DX_UI_DIRTY);
    /* RIGHT is handled and changes nothing. */
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: inv idle");
    CHECK_INT(key(DX_UI_DIAL), DX_UI_IGNORED);
    /* LEFT goes back, with the same row selected. */
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK_STR(last_log(), "deitrix: inv back row=1");
    draw();
    /* The row shows the mark and the new count; the weight stays. */
    CHECK(strstr(fake.draws, "rect 0 64 320 16 fg fill\n"
                             "rect 4 70 4 4 bg fill\n"
                             "text 284 64 \"21 m\" bg fg s1\n"
                             "text 12 64 \"Rope\" bg fg s1\n") != NULL);
    CHECK(has_text("2 entries, 1.5 kg"));
    /* The Back button goes back too. */
    key(DX_UI_SELECT);
    CHECK_INT(key(DX_UI_BACK), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK_STR(last_log(), "deitrix: inv back row=1");
    CHECK_INT(it->changes, changes + 2);
}

/* Through the app shell: routing, sounds and the tab change. */
static void push_key(uint32_t btn)
{
    fake_push(DX_EV_BUTTON, btn, DX_PRESS_DOWN);
    fake_push(DX_EV_BUTTON, btn, DX_PRESS_RELEASE);
}

static void steps(dx_app_t *app, int n)
{
    while (n-- > 0) {
        dx_app_step(app);
    }
}

TEST(shell_routing_and_leave)
{
    dx_app_t app;
    int tones;

    load(SMALL);
    CHECK_INT(dx_app_init(&app, dx_rom_screens, dx_rom_screen_count), 0);
    dx_app_set_tab(&app, TAB_INV);
    push_key(DX_BTN_DOWN);
    push_key(DX_BTN_SELECT);
    push_key(DX_BTN_RIGHT);  /* stays in the detail */
    push_key(DX_BTN_UP);
    steps(&app, 9);
    CHECK_INT(app.tab, TAB_INV);
    CHECK(strstr(fake.log, "deitrix: inv open=rope\ndeitrix: inv idle\n"
                           "deitrix: inv count=21\n") != NULL);
    CHECK(fake_find_text("UP/DOWN: +/-  SELECT: mark  BACK: list") != NULL);
    /* A refused press denies with a tone; a refused repeat is silent. */
    dx_items_set_count(dx_content_items(), 1, 999);
    tones = fake.tone_count;
    push_key(DX_BTN_UP);
    steps(&app, 2);
    CHECK_INT(fake.tone_count, tones + 1);
    fake_push(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_REPEAT);
    steps(&app, 1);
    CHECK_INT(fake.tone_count, tones + 1);
    /* The dial leaves from the detail; the way back shows the list at
     * the same category and row. */
    fake_push(DX_EV_DIAL, 0, 1);
    fake_push(DX_EV_DIAL, 0, -1);
    fake_clear_draws();
    steps(&app, 3);
    CHECK_INT(app.tab, TAB_INV);
    CHECK(strstr(fake.log, "deitrix: tab=DATA\ndeitrix: tab=INV\n") != NULL);
    CHECK(fake_find_text("UP/DOWN: move  SELECT: open") != NULL);
    CHECK(fake_find_text("999 m") != NULL);
    CHECK_INT(fake_find_text("Rope")->bg, fake.theme.fg);
    /* LEFT at the first category changes the tab. */
    push_key(DX_BTN_LEFT);
    steps(&app, 2);
    CHECK_INT(app.tab, 0);
}

/* Six words of 20 letters: 125 characters, one word per line. */
#define W20    "Wwwwwwwwwwwwwwwwwwww"
#define WORDS6 W20 " " W20 " " W20 " " W20 " " W20 " " W20

/* 5 categories, 64 items: the longest names and units, the most rows. */
static void load_limits(void)
{
    int n = 0;
    int i;

    n += tfmt(text + n, sizeof(text) - (size_t)n,
              "version = 1\n[categories]\na = ABCDE\nb = FGHIJ\nc = KLMNO\nd = PQRST\n"
              "e = UVWXY\n");
    for (i = 0; i < 64; i++) {
        /* 60 in a, then one each in b, d and e: c is empty. */
        const char *cat = i < 60 ? "a" : i == 60 ? "b" : i == 61 ? "d" : "e";

        n += tfmt(text + n, sizeof(text) - (size_t)n,
                  "[item.i%d]\nname = Item %02d with the longest name ok\ncat = %s\n"
                  "count = 999\nunit = abcdef\nweight = 999999\n"
                  "note = %s\nnote2 = %s\n",
                  i, i, cat,
                  WORDS6, WORDS6);
    }
    load(text);
    CHECK_INT(dx_content_items()->item_count, 64);
    CHECK_INT(dx_content_items()->cat_count, 5);
}

TEST(limits_stay_inside_the_body)
{
    int i, cat;

    load_limits();
    draw();
    CHECK(inside_body());
    CHECK(has_text("60 entries, 59999.9 kg"));
    /* The name is cut before the value; the value is whole. */
    CHECK(has_text("Item 00 with the longest ~"));
    CHECK(has_text("999 abcdef"));
    /* Down to the end of 60 rows, then refused. */
    for (i = 1; i < 60; i++) {
        fake.log[0] = '\0';
        CHECK_INT(send(DX_UI_DOWN, 1), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_ROW_MOVED);
    }
    CHECK_STR(last_log(), "deitrix: inv cat=0 row=59");
    CHECK_INT(key(DX_UI_DOWN), DX_UI_IGNORED);
    draw();
    CHECK(inside_body());
    CHECK(has_text("Item 51 with the longest ~"));
    CHECK(has_text("Item 59 with the longest ~"));
    CHECK(!has_text("Item 50 with the longest ~"));
    /* Every item's detail: inside, with the note cut by the marker. */
    for (i = 0; i < 60; i++) {
        fake.log[0] = '\0';
        if (i > 0) {
            send(DX_UI_UP, 1);
        }
    }
    for (cat = 0; cat < 5; cat++) {
        for (i = 0; i < 60; i++) {
            fake.log[0] = '\0';
            if (key(DX_UI_SELECT) == DX_UI_IGNORED) {
                break;
            }
            draw();
            CHECK(inside_body());
            CHECK(has_text("999 abcdef"));
            CHECK(has_text("1000.0 kg"));
            CHECK(fake.texts[fake.text_count - 1].text[strlen(fake.texts[fake.text_count - 1].text) - 1] == '~');
            key(DX_UI_BACK);
            if (key(DX_UI_DOWN) == DX_UI_IGNORED) {
                break;
            }
        }
        draw();
        CHECK(inside_body());
        key(DX_UI_RIGHT);
    }
    /* The last category, UVWXY, holds two items. */
    CHECK_STR(last_log(), "deitrix: inv end row=1");
}

/* The held-key rule through the shell: a held SELECT opens the detail
 * once, a held LEFT or BACK closes it once, and the category and the
 * tab stay. */
TEST(held_keys_open_and_close_once)
{
    static const uint32_t backs[2] = { DX_BTN_LEFT, DX_BTN_BACK };
    dx_app_t app;
    int i;

    for (i = 0; i < 2; i++) {
        load(SMALL);
        CHECK_INT(dx_app_init(&app, dx_rom_screens, dx_rom_screen_count), 0);
        dx_app_set_tab(&app, TAB_INV);
        dx_app_step(&app);
        steps(&app, fake_hold(DX_BTN_RIGHT, 0));
        fake.log[0] = '\0';
        steps(&app, fake_hold(DX_BTN_SELECT, 12));
        CHECK_STR(fake.log, "deitrix: inv open=bread\n");
        fake.log[0] = '\0';
        steps(&app, fake_hold(backs[i], 12));
        CHECK_INT(app.tab, TAB_INV);
        CHECK_STR(fake.log, "deitrix: inv back row=0\n");
        CHECK_STR(inv->hint(inv->st), "UP/DOWN: move  SELECT: open");
        /* The next fresh press acts again: FOOD to TOOLS. */
        steps(&app, fake_hold(DX_BTN_LEFT, 0));
        CHECK_STR(last_log(), "deitrix: inv cat=0 row=0");
        CHECK_INT(app.tab, TAB_INV);
    }
}
