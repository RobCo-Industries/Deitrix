/* The DATA tab: NOTES, TAPES and CLOCK through the app shell and the fake. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "content.h"
#include "fake_platform.h"
#include "screens/data_view.h"
#include "screens/screens.h"
#include "services/services.h"
#include "ui/frame.h"
#include "ui/text.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))
#define NOTES_DIR "/PIPBOY/DATA/NOTES/"
#define DATA_TAB 2

static dx_app_t app;

/* Boots the shell on DATA with whatever the card holds. */
static void boot(void)
{
    dx_content_load();
    dx_data_reset();
    CHECK_INT(dx_app_init(&app, dx_rom_screens, dx_rom_screen_count), 0);
    dx_app_set_tab(&app, DATA_TAB);
    fake.log[0] = '\0';
    dx_app_step(&app);
}

static const char *last_log(void);

static void key_ev(int type, int repeat)
{
    dx_ui_event_t ev = { type, repeat, 0 };

    /* Long walks: keep only the last line of the screen's log. */
    if (strlen(fake.log) > FAKE_LOG_SIZE / 2) {
        char keep[256];

        tfmt(keep, sizeof(keep), "%s", last_log());
        tfmt(fake.log, sizeof(fake.log), "%s\n", keep);
    }

    if (type == DX_UI_DIAL) {
        ev.delta = 1;
    }
    fake_clear_draws();
    dx_app_route(&app, &ev);
    dx_app_step(&app);
}

static void key(int type)
{
    key_ev(type, 0);
}

static void draw_body(void)
{
    dx_ctx_t c = fake_ctx();

    fake_clear_draws();
    dx_screen_data.draw(NULL, &c, dx_frame_body());
}

static int body_inside(void)
{
    return fake_points_inside(dx_frame_body());
}

static const char *hint(void)
{
    return dx_screen_data.hint(NULL);
}

static void add_note(const char *file, const char *text)
{
    char path[FAKE_PATH_MAX];

    tfmt(path, sizeof(path), NOTES_DIR "%s", file);
    fake_add_copy(path, text, (uint32_t)strlen(text));
}

/* The last "deitrix: data" line of the log, without its newline, or "". */
static const char *last_log(void)
{
    static char line[256];
    const char *p = fake.log, *last = NULL, *end;

    while ((p = strstr(p, "deitrix: data ")) != NULL) {
        if (p == fake.log || p[-1] == '\n') {
            last = p;
        }
        p++;
    }
    if (last == NULL) {
        return "";
    }
    end = strchr(last, '\n');
    tfmt(line, sizeof(line), "%.*s", (int)(end != NULL ? end - last : (long)strlen(last)), last);
    return line;
}

/* ---- NOTES -------------------------------------------------------------- */

TEST(notes_none)
{
    fake_reset();
    boot();
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=0");
    draw_body();
    CHECK(fake_find_text("No notes on the card.") != NULL);
    CHECK(body_inside());
    CHECK_STR(hint(), "RIGHT: TAPES   LEFT or DIAL: tab");
    /* SELECT, UP, DOWN and Back do nothing. */
    fake.log[0] = '\0';
    key(DX_UI_SELECT);
    key(DX_UI_DOWN);
    key(DX_UI_UP);
    key(DX_UI_BACK);
    CHECK_STR(fake.log, "");
    CHECK_INT(app.tab, DATA_TAB);
    /* LEFT at the first sub-tab changes the tab. */
    key(DX_UI_LEFT);
    CHECK_INT(app.tab, 1);
}

TEST(notes_folder_error)
{
    fake_reset();
    add_note("A.TXT", "Title\nBody\n");
    fake.read_fail = 1;
    boot();
    fake.read_fail = 0;
    draw_body();
    CHECK(fake_find_text("The notes were not read.") != NULL);
    CHECK(fake_find_text("No notes on the card.") == NULL);
    CHECK(fake_find_text("NOTES read error") != NULL);
    CHECK_STR(hint(), "RIGHT: TAPES   LEFT or DIAL: tab");
    CHECK(body_inside());
}

TEST(notes_one_open_and_back)
{
    const fake_text_t *t;

    fake_reset();
    add_note("ONLY.TXT", "A single note\nFirst line of the body.\nSecond line.\n");
    boot();
    CHECK_STR(hint(), "UP/DOWN: move   SELECT: open");
    draw_body();
    t = fake_find_text("A single note");
    CHECK(t != NULL && t->y == 42 + 6 && t->bg == fake.theme.fg);
    /* DOWN at the only row: the list stays. */
    fake.log[0] = '\0';
    key(DX_UI_DOWN);
    CHECK_STR(fake.log, "");
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data note ONLY.TXT top=0 lines=2");
    CHECK_STR(hint(), "BACK/LEFT: list   DIAL: tab");
    draw_body();
    t = fake_find_text("A single note");
    CHECK(t != NULL && t->x == 4 && t->y == 42 + 4 && t->bg == DX_COLOR_NONE);
    t = fake_find_text("First line of the body.");
    CHECK(t != NULL && t->x == 4 && t->y == 42 + 26);
    CHECK(fake_find_text("Second line.") != NULL);
    CHECK(body_inside());
    /* In the note: UP, DOWN, RIGHT and SELECT do not leave it or the tab. */
    key(DX_UI_RIGHT);
    key(DX_UI_DOWN);
    key(DX_UI_UP);
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(last_log(), "deitrix: data note ONLY.TXT top=0 lines=2");
    /* LEFT goes back to the list. */
    key(DX_UI_LEFT);
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=1");
    /* So does Back. */
    key(DX_UI_SELECT);
    key(DX_UI_BACK);
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=1");
}

static void add_32(void)
{
    char file[16], text[64];
    int i;

    for (i = 0; i < 32; i++) {
        tfmt(file, sizeof(file), "N%02d.TXT", i);
        tfmt(text, sizeof(text), "%s %02d%s\nBody %d\n", i == 31 ? "Thirty chars note title" : "Note", i,
             i == 31 ? "xxxxx" : "", i);
        add_note(file, text);
    }
}

TEST(notes_32_scroll_and_back_keeps_the_selection)
{
    int i;

    fake_reset();
    add_32();
    boot();
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=32");
    draw_body();
    CHECK(body_inside());
    /* Ten rows fit under the sub-tabs. */
    CHECK(fake_find_text("Note 09") != NULL);
    CHECK(fake_find_text("Note 10") == NULL);
    for (i = 1; i < 32; i++) {
        key(DX_UI_DOWN);
    }
    CHECK_STR(last_log(), "deitrix: data notes sel=31 top=22 count=32");
    draw_body();
    CHECK(body_inside());
    CHECK(fake_find_text("Thirty chars note title 31xxxxx") == NULL);
    /* The title is cut at 30 characters by the model. */
    CHECK(fake_find_text("Thirty chars note title 31xxxx") != NULL);
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data notes sel=31 top=22 count=32");
    key(DX_UI_UP);
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data note N30.TXT top=0 lines=1");
    key(DX_UI_BACK);
    CHECK_STR(last_log(), "deitrix: data notes sel=30 top=22 count=32");
    /* A tab change keeps the selection and closes the note. */
    key(DX_UI_SELECT);
    key(DX_UI_DIAL);
    CHECK_INT(app.tab, DATA_TAB + 1);
    dx_app_set_tab(&app, DATA_TAB);
    dx_app_step(&app);
    CHECK_STR(last_log(), "deitrix: data notes sel=30 top=22 count=32");
}

/* A body of `lines` wrapped lines of 38 characters. */
static void long_note(const char *file, int lines)
{
    static char text[DX_NOTE_FILE_MAX + 1];
    int n, i;

    n = tfmt(text, sizeof(text), "Long note\n");
    for (i = 0; i < lines && n + 39 < (int)sizeof(text); i++) {
        memset(text + n, 'a' + i % 26, 38);
        text[n + 38] = '\n';
        n += 39;
    }
    text[n] = '\0';
    add_note(file, text);
}

TEST(note_scrolls_and_is_cut)
{
    int i;

    fake_reset();
    /* 4096 bytes: 10 + 104 lines of 39 = 4066; fits the 128 lines. */
    long_note("A.TXT", 104);
    /* 128 short lines and more: over the wrapped-line limit. */
    {
        static char text[DX_NOTE_FILE_MAX];
        int n = tfmt(text, sizeof(text), "Many lines\n");

        for (i = 0; i < 140; i++) {
            n += tfmt(text + n, sizeof(text) - (size_t)n, "%d\n", i);
        }
        add_note("B.TXT", text);
    }
    boot();
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data note A.TXT top=0 lines=104");
    CHECK_STR(hint(), "UP/DOWN: scroll   BACK/LEFT: list");
    draw_body();
    CHECK(body_inside());
    for (i = 0; i < 200; i++) {
        key_ev(DX_UI_DOWN, 1);
    }
    CHECK_STR(last_log(), "deitrix: data note A.TXT top=95 lines=104");
    draw_body();
    CHECK(body_inside());
    key(DX_UI_LEFT);
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data note B.TXT top=0 lines=128 cut");
    for (i = 0; i < 200; i++) {
        key_ev(DX_UI_DOWN, 1);
    }
    draw_body();
    CHECK(fake_find_text(DX_TEXT_CUT_MARK) != NULL);
    CHECK(body_inside());
}

TEST(note_read_error)
{
    fake_reset();
    add_note("A.TXT", "Gone soon\nBody\n");
    boot();
    fake_remove_file(NOTES_DIR "A.TXT");
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data note A.TXT error top=0 lines=0");
    draw_body();
    CHECK(fake_find_text("Gone soon") != NULL);
    CHECK(fake_find_text("The note could not be read.") != NULL);
    CHECK(body_inside());
    key(DX_UI_BACK);
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=1");
}

TEST(notes_body_draw_list)
{
    fake_reset();
    add_note("A.TXT", "Walk\nCool morning.\n");
    boot();
    key(DX_UI_SELECT);
    draw_body();
    CHECK(strstr(fake.draws, "text 4 23 \"NOTES\" fg none s1\n") != NULL);
    CHECK(strstr(fake.draws, "text 4 46 \"Walk\" fg none s1\n"
                             "line 4 64 315 64 faint\n"
                             "text 4 68 \"Cool morning.\" fg none s1\n") != NULL);
}
