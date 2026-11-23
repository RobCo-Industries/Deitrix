/* The DATA tab: NOTES, TAPES and CLOCK through the app shell and the fake. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "content.h"
#include "fake_platform.h"
#include "model/data_time.h"
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
    /* RADIO, two detents on, shows nothing on air and no failure. */
    fake.log[0] = '\0';
    key(DX_UI_DIAL);
    key(DX_UI_DIAL);
    CHECK_INT(app.tab, DATA_TAB + 2);
    CHECK(strstr(fake.log, "radio shows Cannot play") == NULL);
    {
        dx_ctx_t c = fake_ctx();

        fake_clear_draws();
        dx_screen_radio.draw(NULL, &c, dx_frame_body());
        CHECK(fake_find_text("Radio off.") != NULL);
        CHECK(fake_find_text("Station has nothing to play.") == NULL);
    }
}

/* ---- TAPES -------------------------------------------------------------- */

static const dx_service_t *const services[] = { &dx_service_radio, &dx_service_autosave };

/* The sample card, the services, then DATA on TAPES. */
static void boot_tapes(void)
{
    CHECK(fake_load_tree("content", "") > 0);
    boot();
    dx_radio_attach(dx_content_radio());
    dx_autosave_attach(dx_content_settings(), dx_content_items());
    CHECK_INT(dx_app_start_services(&app, services, 2), 0);
    key(DX_UI_RIGHT);
}

static uint8_t icon[DX_HOLO_ICON_SIZE];

TEST(tapes_none_and_list_error)
{
    fake_reset();
    boot_tapes();
    CHECK_INT(fake.holo_lists, 1);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=0 | ");
    CHECK_STR(hint(), "LEFT/RIGHT: page   DIAL: tab");
    draw_body();
    CHECK(fake_find_text("No holotapes on the card.") != NULL);
    CHECK(body_inside());
    key(DX_UI_SELECT);
    key(DX_UI_DOWN);
    CHECK_INT(fake.launch_count, 0);
    /* A listing error: a plain message. */
    fake.holo_list_rc = DX_EIO;
    key(DX_UI_LEFT);
    key(DX_UI_RIGHT);
    CHECK_INT(fake.holo_lists, 2);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=-5 | ");
    draw_body();
    CHECK(fake_find_text("The holotapes could not be read.") != NULL);
    CHECK(body_inside());
}

TEST(tapes_list_kinds_titles_and_icon)
{
    const fake_text_t *t;
    int i;

    fake_reset();
    for (i = 0; i < DX_HOLO_ICON_SIZE; i++) {
        icon[i] = (uint8_t)(i % 4 == 0 ? 0x80 : 0);
    }
    fake_add_holo("BEACON.HTP", DX_HOLO_GAME, "Beacon sample tape", icon);
    fake_add_holo("NOTITLE.HTP", DX_HOLO_UTILITY, "", NULL);
    fake_add_holo("RECORDS.HTP", DX_HOLO_DATA, "Old records", NULL);
    fake_add_holo("ODD.HTP", 7,
                  "A title as long as the kernel allows: sixty-three characters!!", NULL);
    boot_tapes();
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=4 | ");
    CHECK_STR(hint(), "UP/DOWN: move   SELECT: run tape");
    draw_body();
    CHECK(body_inside());
    t = fake_find_text("Beacon sample tape");
    CHECK(t != NULL && t->bg == fake.theme.fg && t->y == 42 + 6);
    CHECK(fake_find_text("NOTITLE.HTP") != NULL);
    CHECK(fake_find_text("Old records") != NULL);
    CHECK(fake_find_text("GAME") != NULL);
    CHECK(fake_find_text("UTIL") != NULL);
    CHECK(fake_find_text("DATA") != NULL || strstr(fake.draws, "\"DATA\"") != NULL);
    CHECK(fake_find_text("?") != NULL);
    /* The kind sits left of the icon column. */
    CHECK_INT(fake_find_text("UTIL")->x + 4 * 8, 320 - 40 - 4);
    /* The icon of the selected tape at the top right. */
    CHECK(strstr(fake.draws, "blit1 284 48 32 32 stride 4 fg none") != NULL);
    key(DX_UI_DOWN);
    draw_body();
    CHECK(strstr(fake.draws, "blit1") == NULL);
    CHECK(body_inside());
}

TEST(tapes_launch_order_and_results)
{
    static const struct {
        int32_t rc;
        const char *line;
    } table[] = {
        { 0, "Tape ended, status 0." },
        { 255, "Tape ended, status 255." },
        { DX_EFAULT, "Tape was stopped by the system." },
        { DX_ENOEXEC, "Not a holotape." },
        { DX_EIO, "Card error." },
        { DX_ENOENT, "Tape not found." },
        { DX_EPERM, "The tape could not start." },
    };
    char want[128];
    size_t i;

    fake_reset();
    fake_add_holo("BEACON.HTP", DX_HOLO_GAME, "Beacon", NULL);
    fake_add_holo("TOOL.HTP", DX_HOLO_UTILITY, "Tool", NULL);
    boot_tapes();
    key(DX_UI_DOWN);
    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        int lists = fake.holo_lists;

        /* The radio plays and a change waits for the autosave. */
        CHECK_INT(dx_radio_play(0, fake.ticks), 0);
        CHECK_INT(dx_radio_state(), DX_RADIO_TUNE);
        CHECK(dx_items_toggle_marked(dx_content_items(), 0));
        fake.launch_rc = table[i].rc;
        fake.launch_draw_ms = 5000;
        fake.log[0] = '\0';
        {
            int writes = fake.save_writes;
            int stops = fake_audio_ops(FAKE_AU_STOP);

            key(DX_UI_SELECT);
            CHECK_INT(fake.launch_count, (int)i + 1);
            CHECK_STR(fake.launch_file, "TOOL.HTP");
            CHECK_STR(fake.launch_args, "");
            /* The radio stopped and the save was written before the call. */
            CHECK_INT(fake.launch_stops_at, stops + 1);
            CHECK_INT(fake.launch_writes_at, writes + 1);
            CHECK(!dx_autosave_pending());
        }
        /* After it: the radio stays off, the list was read again. */
        CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
        CHECK_INT(dx_radio_current(), -1);
        /* A stop for a launch is not a failure. */
        CHECK_INT(dx_radio_failed(), -1);
        CHECK_INT(fake.holo_lists, lists + 1);
        /* The save line, then the launch and its result. */
        tfmt(want, sizeof(want),
             "deitrix: save INV.SAV ok\ndeitrix: data launch TOOL.HTP\n"
             "deitrix: data tape rc=%d\n", (int)table[i].rc);
        CHECK(strncmp(fake.log, want, strlen(want)) == 0);
        tfmt(want, sizeof(want), "deitrix: data tapes sel=1 top=0 count=2 | %s", table[i].line);
        CHECK_STR(last_log(), want);
        /* The whole frame was drawn again. */
        CHECK_INT(fake.clears, 1);
        CHECK_INT(fake.presents, 1);
        draw_body();
        CHECK(fake_find_text(table[i].line) != NULL);
        CHECK(body_inside());
        /* No tone was blocked by a radio that thinks it plays. */
        CHECK_INT(app.tab, DATA_TAB);
    }
    /* The line goes when the selection moves. */
    key(DX_UI_UP);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=2 | ");
    /* And when the sub-tab is left. */
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=2 | The tape could not start.");
    key(DX_UI_RIGHT);
    key(DX_UI_LEFT);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=2 | ");
    /* And when the tab is left. */
    key(DX_UI_SELECT);
    key(DX_UI_DIAL);
    dx_app_set_tab(&app, DATA_TAB);
    dx_app_step(&app);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=2 | ");
}

/* A failed save write stops the first SELECT and keeps the radio on.
 * A second fresh press runs the tape anyway. */
TEST(tapes_failed_save_asks_for_a_second_press)
{
    const char *line = "Save failed. SELECT again runs it.";
    int stops;

    CHECK_INT((int)strlen(line), 34);
    fake_reset();
    fake_add_holo("BEACON.HTP", DX_HOLO_GAME, "Beacon", NULL);
    fake_add_holo("TOOL.HTP", DX_HOLO_UTILITY, "Tool", NULL);
    boot_tapes();
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    CHECK(dx_items_toggle_marked(dx_content_items(), 0));
    fake.save_fail = 100;
    stops = fake_audio_ops(FAKE_AU_STOP);
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_TUNE);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), stops);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=2 | Save failed. SELECT again runs it.");
    draw_body();
    CHECK(fake_find_text(line) != NULL);
    CHECK_STR(hint(), "UP/DOWN: move   SELECT: run tape");
    CHECK(body_inside());
    /* A repeat of the held key is not the second press. */
    key_ev(DX_UI_SELECT, 1);
    CHECK_INT(fake.launch_count, 0);
    /* Moving the selection drops the line and the confirmation. */
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data tapes sel=1 top=0 count=2 | ");
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 0);
    CHECK_STR(last_log(), "deitrix: data tapes sel=1 top=0 count=2 | Save failed. SELECT again runs it.");
    /* The second fresh press runs it, though the save fails again. */
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 1);
    CHECK_STR(fake.launch_file, "TOOL.HTP");
    CHECK_INT(fake.launch_stops_at, stops + 1);
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    /* A save that works runs the tape at once. */
    fake.save_fail = 0;
    key(DX_UI_UP);
    CHECK(dx_items_toggle_marked(dx_content_items(), 0));
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 2);
    CHECK_STR(fake.launch_file, "BEACON.HTP");
    CHECK(!dx_autosave_pending());
    /* Leaving the sub-tab drops the confirmation. */
    CHECK(dx_items_toggle_marked(dx_content_items(), 0));
    fake.save_fail = 100;
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 2);
    key(DX_UI_RIGHT);
    key(DX_UI_LEFT);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=2 | ");
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 2);
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 3);
    /* Nothing pending runs the tape at once, even with a bad card. */
    fake.save_fail = 0;
    CHECK(dx_items_toggle_marked(dx_content_items(), 0));
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 4);
    fake.save_fail = 100;
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 5);
    fake.save_fail = 0;
}

/* After the autosave gave up, the change is still not on the card: the
 * first SELECT refuses and the second runs the tape. */
TEST(tapes_save_gave_up_asks_for_a_second_press)
{
    int i;

    fake_reset();
    fake_add_holo("BEACON.HTP", DX_HOLO_GAME, "Beacon", NULL);
    boot_tapes();
    fake.save_fail = 1000000;
    CHECK(dx_items_toggle_marked(dx_content_items(), 0));
    key(DX_UI_UP); /* any key wakes the services, as INV's mark key does */
    fake.log[0] = '\0';
    for (i = 0; i < 60; i++) {
        fake.ticks += 1000;
        dx_app_step(&app);
    }
    CHECK(strstr(fake.log, "deitrix: save gave up\n") != NULL);
    CHECK(!dx_autosave_pending());
    CHECK(dx_autosave_unsaved());
    CHECK_INT(fake.launch_count, 0);
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 0);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=1 | Save failed. SELECT again runs it.");
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 1);
    CHECK_STR(fake.launch_file, "BEACON.HTP");
    CHECK(dx_autosave_unsaved());
    fake.save_fail = 0;
}

TEST(tapes_list_changes_after_a_launch)
{
    fake_reset();
    fake_add_holo("A.HTP", DX_HOLO_GAME, "A", NULL);
    fake_add_holo("B.HTP", DX_HOLO_GAME, "B", NULL);
    fake_add_holo("C.HTP", DX_HOLO_GAME, "C", NULL);
    boot_tapes();
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    /* The tape vanished meanwhile, and A with it. */
    fake.launch_rc = DX_ENOENT;
    fake.holo_count = 0;
    fake_add_holo("B.HTP", DX_HOLO_GAME, "B", NULL);
    fake_add_holo("C.HTP", DX_HOLO_GAME, "C", NULL);
    key(DX_UI_SELECT);
    /* C stays selected, now the second row. */
    CHECK_STR(last_log(), "deitrix: data tapes sel=1 top=0 count=2 | Tape not found.");
    fake.holo_count = 0;
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=0 | Tape not found.");
    draw_body();
    CHECK(fake_find_text("No holotapes on the card.") != NULL);
    CHECK(fake_find_text("Tape not found.") != NULL);
    CHECK(body_inside());
}

TEST(tapes_data_tape_is_refused)
{
    fake_reset();
    fake_add_holo("RECORDS.HTP", DX_HOLO_DATA, "Records", NULL);
    boot_tapes();
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_TUNE);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=1 | Data tapes cannot be started.");
    /* A tape listed as a game that the kernel refuses as a data tape
     * would show the same: only the kind decides. */
    fake.holos[0].kind = DX_HOLO_GAME;
    key(DX_UI_LEFT);
    key(DX_UI_RIGHT);
    fake.launch_rc = DX_ENOEXEC;
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 1);
    CHECK_STR(last_log(), "deitrix: data tapes sel=0 top=0 count=1 | Not a holotape.");
    dx_radio_stop();
}

TEST(tapes_scroll_32)
{
    char file[16];
    int i;

    fake_reset();
    for (i = 0; i < DX_HOLO_MAX; i++) {
        tfmt(file, sizeof(file), "T%02d.HTP", i);
        fake_add_holo(file, DX_HOLO_GAME, "", NULL);
    }
    boot_tapes();
    for (i = 0; i < 40; i++) {
        key_ev(DX_UI_DOWN, 1);
    }
    CHECK_STR(last_log(), "deitrix: data tapes sel=31 top=23 count=32 | ");
    fake.launch_rc = 3;
    key(DX_UI_SELECT);
    draw_body();
    CHECK(fake_find_text("T31.HTP") != NULL);
    CHECK(fake_find_text("Tape ended, status 3.") != NULL);
    CHECK(body_inside());
}

/* ---- CLOCK -------------------------------------------------------------- */

#define T_2026_11_23_093007 1795426207u /* a Monday */

static void boot_clock(void)
{
    boot();
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
}

TEST(clock_page_and_tick)
{
    unsigned flags = 0;
    const fake_text_t *t;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T_2026_11_23_093007;
    boot_clock();
    CHECK_STR(last_log(), "deitrix: data clock 09:30:07 2026-11-23 Monday | ");
    CHECK_STR(hint(), "SELECT: set time   LEFT/RIGHT: page");
    draw_body();
    CHECK(body_inside());
    CHECK(fake_find_text("2026-11-23  Monday") != NULL);
    CHECK(fake_find_text("The clock runs in UTC.") != NULL);
    /* The big digits: unit 6 strokes, centred: 42 units = 252 px. */
    CHECK(strstr(fake.draws, "line ") != NULL);
    t = fake_find_text("2026-11-23  Monday");
    CHECK_INT(t->x, (320 - 18 * 8) / 2);
    /* The tick: every 250 ms, a redraw only when the second changed. */
    fake.ticks = 5000;
    CHECK_INT(dx_screen_data.tick(NULL, 5000, &flags), 5250);
    CHECK_INT(flags, 0);
    fake.clock++;
    CHECK_INT(dx_screen_data.tick(NULL, 5250, &flags), 5500);
    CHECK_INT(flags, DX_UI_DIRTY);
    dx_app_step(&app);
    CHECK_STR(last_log(), "deitrix: data clock 09:30:08 2026-11-23 Monday | ");
    flags = 0;
    CHECK_INT(dx_screen_data.tick(NULL, 5500, &flags), 5750);
    CHECK_INT(flags, 0);
    /* The clock going away redraws too. */
    fake.clock_valid = 0;
    CHECK_INT(dx_screen_data.tick(NULL, 5750, &flags), 6000);
    CHECK_INT(flags, DX_UI_DIRTY);
    /* The tick wraps at 2^31 through the helper and never returns 0. */
    flags = 0;
    CHECK_INT(dx_screen_data.tick(NULL, 0x7fffff80u, &flags), 122u);
    CHECK_INT(dx_screen_data.tick(NULL, 0x7fffff06u, &flags), 1u);
    /* Another sub-tab stops the tick. */
    key(DX_UI_LEFT);
    flags = 0;
    CHECK_INT(dx_screen_data.tick(NULL, 7000, &flags), 0);
    CHECK_INT(flags, 0);
}

TEST(clock_none)
{
    fake_reset();
    boot_clock();
    CHECK_STR(last_log(), "deitrix: data clock none | ");
    draw_body();
    CHECK(body_inside());
    CHECK(fake_find_text("No clock. SELECT sets the time.") != NULL);
    CHECK(fake_find_text("The clock runs in UTC.") == NULL);
    /* UP, DOWN and Back do nothing; RIGHT goes to the next tab. */
    fake.log[0] = '\0';
    key(DX_UI_UP);
    key(DX_UI_DOWN);
    key(DX_UI_BACK);
    CHECK_STR(fake.log, "");
    key(DX_UI_RIGHT);
    CHECK_INT(app.tab, DATA_TAB + 1);
}

TEST(setter_starts_from_the_clock_or_2026)
{
    fake_reset();
    boot_clock();
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2026-01-01 00:00 | ");
    CHECK_STR(hint(), "UP/DN: set  SELECT: store  BACK: cancel");
    draw_body();
    CHECK(body_inside());
    CHECK(fake_find_text("Year") != NULL);
    /* The active field is inverse. */
    CHECK(strstr(fake.draws, "text 32 86 \"2026\" bg none s2\n") != NULL);
    CHECK(strstr(fake.draws, "text 112 86 \"01\" fg none s2\n") != NULL);
    key(DX_UI_BACK);
    fake.clock_valid = 1;
    fake.clock = T_2026_11_23_093007;
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2026-11-23 09:30 | ");
    key(DX_UI_BACK);
    /* A clock outside 2000 to 2099: the default. */
    fake.clock = 100u;
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2026-01-01 00:00 | ");
}

TEST(setter_every_key_in_every_field)
{
    static const char *const up[DX_DT_FIELDS] = {
        "2027-11-23 09:30", "2027-12-23 09:30", "2027-12-24 09:30", "2027-12-24 10:30",
        "2027-12-24 10:31",
    };
    char want[128];
    int f;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T_2026_11_23_093007;
    boot_clock();
    key(DX_UI_SELECT);
    for (f = 0; f < DX_DT_FIELDS; f++) {
        key(DX_UI_UP);
        key_ev(DX_UI_UP, 1);
        key(DX_UI_DOWN);
        tfmt(want, sizeof(want), "deitrix: data setter field=%d %s | ", f, up[f]);
        CHECK_STR(last_log(), want);
        CHECK_INT(app.tab, DATA_TAB);
        draw_body();
        CHECK(body_inside());
        key(DX_UI_RIGHT);
    }
    /* RIGHT on the last field stays there and in the tab. */
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(last_log(), "deitrix: data setter field=4 2027-12-24 10:31 | ");
    /* LEFT walks back; a held LEFT stops at the first field. */
    for (f = 0; f < 6; f++) {
        key_ev(DX_UI_LEFT, 1);
    }
    CHECK_STR(last_log(), "deitrix: data setter field=0 2027-12-24 10:31 | ");
    CHECK(dx_data_clock_nested());
    /* A fresh LEFT on the first field cancels: nothing stored. */
    key(DX_UI_LEFT);
    CHECK(!dx_data_clock_nested());
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_INT(fake.set_time_count, 0);
    CHECK_STR(last_log(), "deitrix: data clock 09:30:07 2026-11-23 Monday | ");
    /* The dial leaves with the setter open; the return shows the page. */
    key(DX_UI_SELECT);
    key(DX_UI_DIAL);
    dx_app_set_tab(&app, DATA_TAB);
    dx_app_step(&app);
    CHECK(!dx_data_clock_nested());
    CHECK_STR(last_log(), "deitrix: data clock 09:30:07 2026-11-23 Monday | ");
}

TEST(setter_wraps_and_clamps)
{
    fake_reset();
    fake.clock_valid = 1;
    fake.clock = 1709208000u; /* 2024-02-29 12:00 */
    boot_clock();
    key(DX_UI_SELECT);
    key(DX_UI_UP); /* 2025: Feb 29 becomes 28 */
    CHECK_STR(last_log(), "deitrix: data setter field=0 2025-02-28 12:00 | ");
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2024-02-28 12:00 | ");
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    key(DX_UI_UP);
    CHECK_STR(last_log(), "deitrix: data setter field=2 2024-02-29 12:00 | ");
    key(DX_UI_UP);
    CHECK_STR(last_log(), "deitrix: data setter field=2 2024-02-01 12:00 | ");
    key(DX_UI_DOWN);
    key(DX_UI_LEFT);
    /* Month 1 down wraps to 12; then day 31 to a 30-day month. */
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data setter field=1 2024-11-29 12:00 | ");
    key(DX_UI_UP);
    key(DX_UI_RIGHT);
    key(DX_UI_UP);
    key(DX_UI_UP);
    CHECK_STR(last_log(), "deitrix: data setter field=2 2024-12-31 12:00 | ");
    key(DX_UI_LEFT);
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data setter field=1 2024-11-30 12:00 | ");
    /* The year wraps 2000 <-> 2099; hour and minute wrap. */
    key(DX_UI_LEFT);
    fake.log[0] = '\0';
    {
        int i;

        for (i = 0; i < 24; i++) {
            key(DX_UI_DOWN);
        }
    }
    CHECK_STR(last_log(), "deitrix: data setter field=0 2000-11-30 12:00 | ");
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2099-11-30 12:00 | ");
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    {
        int i;

        for (i = 0; i < 12; i++) {
            key(DX_UI_UP);
        }
    }
    key(DX_UI_RIGHT);
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data setter field=4 2099-11-30 00:59 | ");
}

TEST(setter_store_ok_failed_and_refused)
{
    fake_reset();
    boot_clock();
    key(DX_UI_SELECT);
    key(DX_UI_UP);
    /* A failed chip write: the setter stays open with the message. */
    fake.set_time_rc = DX_EIO;
    key(DX_UI_SELECT);
    CHECK_INT(fake.set_time_count, 1);
    CHECK_INT(fake.set_times[0], 1798761600u); /* 2027-01-01 00:00:00 */
    CHECK(dx_data_clock_nested());
    CHECK_STR(last_log(),
              "deitrix: data setter field=0 2027-01-01 00:00 | The clock chip did not take the time.");
    draw_body();
    CHECK(fake_find_text("The clock chip did not take the time.") != NULL);
    CHECK(body_inside());
    /* A key clears the message. */
    key(DX_UI_DOWN);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2026-01-01 00:00 | ");
    /* Another error. */
    fake.set_time_rc = DX_EPERM;
    key(DX_UI_SELECT);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2026-01-01 00:00 | The time was not set.");
    /* Stored: the page, with the line about the chip. Seconds are 0. */
    fake.set_time_rc = 0;
    fake.clock_valid = 0;
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    key(DX_UI_UP);
    key(DX_UI_SELECT);
    CHECK_INT(fake.set_time_count, 3);
    CHECK_INT(fake.set_times[2], 1767225600u + 3600u);
    CHECK(strstr(fake.log, "deitrix: data set_time 1767229200 rc=0\n") != NULL);
    CHECK(!dx_data_clock_nested());
    CHECK_STR(last_log(),
              "deitrix: data clock 01:00:00 2026-01-01 Thursday | "
              "Time set. A clock chip keeps it.");
    draw_body();
    CHECK(fake_find_text("Time set. A clock chip keeps it.") != NULL);
    CHECK(body_inside());
    /* The line goes when the sub-tab is left. */
    key(DX_UI_LEFT);
    key(DX_UI_RIGHT);
    CHECK_STR(last_log(), "deitrix: data clock 01:00:00 2026-01-01 Thursday | ");
}

TEST(hints_fit)
{
    const char *h;

    fake_reset();
    boot();
    h = hint();
    CHECK(strlen(h) <= 40);
    key(DX_UI_RIGHT);
    CHECK(strlen(hint()) <= 40);
    key(DX_UI_RIGHT);
    CHECK(strlen(hint()) <= 40);
    key(DX_UI_SELECT);
    CHECK(strlen(hint()) <= 40);
}

/* ---- Held keys ---------------------------------------------------------- */

/* A press, then n repeats of the same key, as the kernel sends a hold. */
static void hold(int type, int n)
{
    int i;

    key(type);
    for (i = 0; i < n; i++) {
        key_ev(type, 1);
    }
}

TEST(held_select_launches_a_tape_once)
{
    fake_reset();
    fake_add_holo("QUICK.HTP", DX_HOLO_GAME, "Quick", NULL);
    boot_tapes();
    fake.launch_rc = DX_EFAULT;
    hold(DX_UI_SELECT, 8);
    CHECK_INT(fake.launch_count, 1);
    /* A SELECT still held when the tape returns sends only repeats. */
    key_ev(DX_UI_SELECT, 1);
    CHECK_INT(fake.launch_count, 1);
    /* A fresh press runs it again. */
    key(DX_UI_SELECT);
    CHECK_INT(fake.launch_count, 2);
    dx_radio_stop();
}

TEST(held_select_opens_the_setter_and_stores_once)
{
    fake_reset();
    boot_clock();
    hold(DX_UI_SELECT, 8);
    CHECK(dx_data_clock_nested());
    CHECK_INT(fake.set_time_count, 0);
    /* UP repeats still step the field. */
    hold(DX_UI_UP, 3);
    CHECK_STR(last_log(), "deitrix: data setter field=0 2030-01-01 00:00 | ");
    hold(DX_UI_SELECT, 8);
    CHECK_INT(fake.set_time_count, 1);
    CHECK(!dx_data_clock_nested());
    CHECK_INT(app.tab, DATA_TAB);
    /* A repeat with no press before it, as after a tab change by the
     * dial: it neither opens nor stores. */
    key(DX_UI_DOWN);
    key_ev(DX_UI_SELECT, 1);
    CHECK(!dx_data_clock_nested());
    key(DX_UI_SELECT);
    key(DX_UI_UP);
    key_ev(DX_UI_SELECT, 1);
    CHECK(dx_data_clock_nested());
    CHECK_INT(fake.set_time_count, 1);
}

TEST(held_back_or_left_closes_the_setter_once)
{
    fake_reset();
    boot_clock();
    key(DX_UI_SELECT);
    hold(DX_UI_BACK, 8);
    CHECK(!dx_data_clock_nested());
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(hint(), "SELECT: set time   LEFT/RIGHT: page");
    /* LEFT on the first field cancels; its repeats stay on CLOCK. */
    key(DX_UI_SELECT);
    hold(DX_UI_LEFT, 8);
    CHECK(!dx_data_clock_nested());
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(hint(), "SELECT: set time   LEFT/RIGHT: page");
    /* A LEFT held from the second field stops at the first. */
    key(DX_UI_SELECT);
    key(DX_UI_RIGHT);
    hold(DX_UI_LEFT, 8);
    CHECK(dx_data_clock_nested());
    CHECK_INT(fake.set_time_count, 0);
    /* After another key, a BACK repeat alone does not close. */
    key(DX_UI_UP);
    key_ev(DX_UI_BACK, 1);
    CHECK(dx_data_clock_nested());
}

TEST(held_keys_open_and_close_a_note_once)
{
    fake_reset();
    add_note("A.TXT", "Alpha\nBody.\n");
    add_note("B.TXT", "Beta\nBody.\n");
    add_note("C.TXT", "Gamma\nBody.\n");
    boot();
    /* DOWN repeats still move in the list. */
    hold(DX_UI_DOWN, 2);
    CHECK_STR(last_log(), "deitrix: data notes sel=2 top=0 count=3");
    hold(DX_UI_UP, 2);
    hold(DX_UI_SELECT, 8);
    CHECK_STR(last_log(), "deitrix: data note A.TXT top=0 lines=1");
    /* A held LEFT closes the note and stays on NOTES and on DATA. */
    hold(DX_UI_LEFT, 8);
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=3");
    CHECK_STR(hint(), "UP/DOWN: move   SELECT: open");
    /* A held BACK closes it once. */
    key(DX_UI_SELECT);
    hold(DX_UI_BACK, 8);
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=3");
    /* A repeat alone neither opens nor closes a note. */
    key_ev(DX_UI_SELECT, 1);
    CHECK_STR(last_log(), "deitrix: data notes sel=0 top=0 count=3");
    key(DX_UI_SELECT);
    key(DX_UI_DOWN);
    key_ev(DX_UI_BACK, 1);
    key_ev(DX_UI_LEFT, 1);
    CHECK_INT(app.tab, DATA_TAB);
    CHECK_STR(last_log(), "deitrix: data note A.TXT top=0 lines=1");
    key(DX_UI_BACK);
    /* After a fresh press, LEFT acts again: the shell changes the tab. */
    key(DX_UI_LEFT);
    CHECK_INT(app.tab, 1);
}
