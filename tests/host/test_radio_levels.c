/* The levels view of the RADIO tab against the fake platform. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "content.h"
#include "fake_platform.h"
#include "screens/radio_screen.h"
#include "screens/screens.h"
#include "services/services.h"
#include "ui/frame.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

#define INI_PATH  "/PIPBOY/DATA/RADIO.INI"
#define TAB_RADIO 4
#define DENY_HZ   180
#define MOVE_HZ   1400

#define HINT_VOLUME_OLD "UP/DOWN: move  LEFT/RIGHT: volume"
#define HINT_VOLUME     "LEFT/RIGHT: volume  SELECT: levels"
#define HINT_VIEW       "LEFT/RIGHT: level  SELECT: done"

static const dx_screen_t *const radio = &dx_screen_radio;

/* 1 a tune, 2 a looping track. */
static const char SMALL[] =
    "version = 1\n"
    "[station.1]\nname = Morning\nkind = tune\nfile = MORN.TUN\n"
    "[station.2]\nname = Hearth\nkind = pcm\nfile = HEARTH.PCM\n";

/* Loads `ini` (NULL: no file) as the card's RADIO.INI and starts the
 * radio as dx_main does, on a fake kernel of ABI `abi`. */
static void load_at(const char *ini, uint32_t abi)
{
    fake_reset();
    fake.sysinfo.abi_version = abi;
    if (ini != NULL) {
        fake_add_text(INI_PATH, ini);
    }
    fake_add_text("/PIPBOY/DATA/RADIO/MORN.TUN", "version 1\nC4 D4 R/8\n");
    fake_add_fill("/PIPBOY/DATA/RADIO/HEARTH.PCM", 2 * DX_RADIO_CHUNK, 128);
    dx_ui_sound_block(0);
    dx_ui_sound_enable(DX_UI_SOUND_ON);
    dx_content_load();
    dx_content_load_saves(80);
    dx_radio_attach(dx_content_radio());
    dx_service_radio.init(fake.ticks);
    dx_radio_screen_reset();
    dx_autosave_attach(dx_content_settings(), dx_content_items());
    dx_service_autosave.init(fake.ticks);
    fake.log[0] = '\0';
}

static unsigned send(int type, int repeat)
{
    dx_ui_event_t ev = { type, repeat, 0 };

    return radio->event(radio->st, &ev);
}

static unsigned key(int type)
{
    return send(type, 0);
}

static void draw(void)
{
    dx_ctx_t c = fake_ctx();

    fake_clear_draws();
    radio->draw(radio->st, &c, dx_frame_body());
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
    return fake_all_points_inside(b);
}

static int tones_of(uint32_t hz)
{
    int n = 0;
    int i;

    for (i = 0; i < fake.tone_count; i++) {
        n += fake.tones[i][0] == hz;
    }
    return n;
}

static void check_hint(const char *want)
{
    const char *h = radio->hint(radio->st);

    CHECK_STR(h, want);
    CHECK(strlen(h) <= DX_HINT_MAX);
}

/* With no station the cursor starts on the Volume row. */
static void open_view(void)
{
    load_at(NULL, DX_ABI_1_6);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK(dx_radio_levels_is_open());
}

/* Through the app shell with the radio service. */
static const dx_service_t *const radio_only[] = { &dx_service_radio };

static void shell_start(dx_app_t *app)
{
    CHECK_INT(dx_app_init(app, dx_rom_screens, dx_rom_screen_count), 0);
    dx_app_set_tab(app, TAB_RADIO);
    CHECK_INT(dx_app_start_services(app, radio_only, 1), 0);
    dx_app_step(app);
}

static void step(dx_app_t *app)
{
    fake_clear_draws(); /* the draw list keeps one frame */
    dx_app_step(app);
    if (fake.last_timeout != DX_WAIT_FOREVER) {
        fake.ticks = dx_ticks_add(fake.ticks, fake.last_timeout);
    }
}

static void push_key(uint32_t btn)
{
    fake_push(DX_EV_BUTTON, btn, DX_PRESS_DOWN);
    fake_push(DX_EV_BUTTON, btn, DX_PRESS_RELEASE);
}

/* ---- Opening and closing ------------------------------------------------ */

TEST(select_opens_the_view_on_1_6)
{
    load_at(NULL, DX_ABI_1_6);
    CHECK(dx_radio_levels_there());
    check_hint(HINT_VOLUME);
    fake.levels[DX_SOUND_RADIO] = 40;
    fake.levels[DX_SOUND_RECORDINGS] = 55;
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK_INT(fake.level_calls, 4);
    CHECK_INT(fake.level_set_count, 0);
    CHECK_STR(fake.log, "deitrix: radio levels radio=40 sfx=100 recordings=55 games=100\n");
    check_hint(HINT_VIEW);
    /* A repeat of SELECT opens nothing: it is the view's now. */
    CHECK_INT(send(DX_UI_SELECT, 1), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: radio idle");
    CHECK(dx_radio_levels_is_open());
    CHECK_INT(fake.level_calls, 4);
}

TEST(longest_open_line)
{
    open_view();
    CHECK_STR(fake.log, "deitrix: radio levels radio=100 sfx=100 recordings=100 games=100\n");
    CHECK_INT(strlen(fake.log), 65);
}

TEST(a_repeat_of_select_does_not_open)
{
    load_at(NULL, DX_ABI_1_6);
    send(DX_UI_SELECT, 1);
    CHECK(!dx_radio_levels_is_open());
    CHECK_INT(fake.level_calls, 0);
    CHECK_STR(last_log(), "deitrix: radio idle");
}

/* Before 1.6: no view, today's hint, no level call in a walk. */
static void no_view_on(uint32_t abi)
{
    static const int keys[] = { DX_UI_SELECT, DX_UI_UP, DX_UI_SELECT, DX_UI_DOWN, DX_UI_DOWN,
                                DX_UI_SELECT, DX_UI_LEFT, DX_UI_RIGHT, DX_UI_BACK, DX_UI_UP,
                                DX_UI_UP, DX_UI_SELECT, DX_UI_DOWN, DX_UI_SELECT };
    size_t i;

    load_at(SMALL, abi);
    CHECK(!dx_radio_levels_there());
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    check_hint(HINT_VOLUME_OLD);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: radio idle");
    CHECK(!dx_radio_levels_is_open());
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        key(keys[i]);
        draw();
        radio->hint(radio->st);
    }
    dx_radio_stop();
    CHECK_INT(fake.level_calls, 0);
    CHECK(!dx_radio_levels_is_open());
}

TEST(no_view_before_1_6)
{
    no_view_on(DX_ABI_1_5);
    no_view_on(0);
}

TEST(select_and_back_close_left_does_not)
{
    open_view();
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED);
    CHECK(dx_radio_levels_is_open());
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK_STR(last_log(), "deitrix: radio levels closed");
    CHECK(!dx_radio_levels_is_open());
    check_hint(HINT_VOLUME);
    key(DX_UI_SELECT);
    CHECK(dx_radio_levels_is_open());
    /* A repeat of BACK does nothing. */
    CHECK_INT(send(DX_UI_BACK, 1), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: radio idle");
    CHECK(dx_radio_levels_is_open());
    CHECK_INT(key(DX_UI_BACK), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    CHECK(!dx_radio_levels_is_open());
    /* The cursor is on the Volume row: LEFT changes the master. */
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: radio volume=70");
    CHECK_INT(fake.volume, 70);
    CHECK_INT(fake.levels[DX_SOUND_RADIO], 95);
}

TEST(reset_closes_the_view)
{
    open_view();
    dx_radio_screen_reset();
    CHECK(!dx_radio_levels_is_open());
    dx_radio_levels_close(); /* closed: nothing */
    CHECK(!dx_radio_levels_is_open());
}

/* ---- Drawing --------------------------------------------------------------- */

TEST(view_draw_list)
{
    load_at(NULL, DX_ABI_1_6);
    fake.levels[DX_SOUND_RADIO] = 40;
    fake.levels[DX_SOUND_SFX] = 100;
    fake.levels[DX_SOUND_RECORDINGS] = 55;
    fake.levels[DX_SOUND_GAMES] = 0;
    key(DX_UI_SELECT);
    draw();
    CHECK_STR(fake.draws,
              "text 12 23 \"SOUND LEVELS\" fg none s1\n"
              "line 0 41 319 41 faint\n"
              "rect 0 48 320 16 fg fill\n"
              "text 12 48 \"Radio tunes\" bg fg s1\n"
              "text 300 48 \"40\" bg fg s1\n"
              "rect 108 51 176 10 bg outline\n"
              "rect 110 53 68 6 bg fill\n"
              "text 12 64 \"Effects\" dim none s1\n"
              "text 292 64 \"100\" fg none s1\n"
              "rect 108 67 176 10 dim outline\n"
              "rect 110 69 172 6 fg fill\n"
              "text 12 80 \"Recordings\" dim none s1\n"
              "text 300 80 \"55\" fg none s1\n"
              "rect 108 83 176 10 dim outline\n"
              "rect 110 85 94 6 fg fill\n"
              "text 12 96 \"Games\" dim none s1\n"
              "text 308 96 \"0\" fg none s1\n"
              "rect 108 99 176 10 dim outline\n"
              "text 12 120 \"Stations that play a tune\" dim none s1\n");
    CHECK(inside_body());
    /* The last row selected: its line under the rows. */
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    draw();
    CHECK(strstr(fake.draws, "text 12 120 \"Holotape games\" dim none s1\n") != NULL);
    CHECK(strstr(fake.draws, "rect 0 96 320 16 fg fill\n"
                             "text 12 96 \"Games\" bg fg s1\n"
                             "text 308 96 \"0\" bg fg s1\n"
                             "rect 108 99 176 10 bg outline\n"
                             "text 12 120") != NULL);
    CHECK(inside_body());
}

/* ---- Keys ------------------------------------------------------------------ */

TEST(up_down_move_and_stop_at_both_ends)
{
    open_view();
    CHECK_INT(key(DX_UI_UP), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: radio level end row=0");
    CHECK_INT(key(DX_UI_DOWN), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_ROW_MOVED);
    CHECK_STR(last_log(), "deitrix: radio level row=1");
    CHECK_INT(send(DX_UI_DOWN, 1), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_ROW_MOVED);
    CHECK_INT(key(DX_UI_DOWN), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_ROW_MOVED);
    CHECK_STR(last_log(), "deitrix: radio level row=3");
    CHECK_INT(key(DX_UI_DOWN), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: radio level end row=3");
    CHECK_INT(key(DX_UI_UP), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_ROW_MOVED);
    CHECK_STR(last_log(), "deitrix: radio level row=2");
    CHECK_INT(fake.level_set_count, 0);
}

TEST(steps_of_five_by_row_and_kind)
{
    static const uint32_t kinds[] = { DX_SOUND_RADIO, DX_SOUND_SFX, DX_SOUND_RECORDINGS,
                                      DX_SOUND_GAMES };
    static const char *const names[] = { "radio", "sfx", "recordings", "games" };
    char want[64];
    int i;

    load_at(NULL, DX_ABI_1_6);
    for (i = 0; i < 4; i++) {
        fake.levels[kinds[i]] = 83;
    }
    key(DX_UI_SELECT);
    for (i = 0; i < 4; i++) {
        int sets = fake.level_set_count;

        if (i > 0) {
            key(DX_UI_DOWN);
        }
        /* Off the grid: the next multiple of 5 each way. */
        CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED);
        CHECK_INT(fake.levels[kinds[i]], 85);
        tfmt(want, sizeof(want), "deitrix: radio level %s=85", names[i]);
        CHECK_STR(last_log(), want);
        CHECK_INT(send(DX_UI_RIGHT, 1), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED);
        CHECK_INT(fake.levels[kinds[i]], 90);
        CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED);
        CHECK_INT(send(DX_UI_LEFT, 1), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED);
        CHECK_INT(fake.levels[kinds[i]], 80);
        CHECK_INT(fake.level_set_count, sets + 4);
        CHECK_INT(fake.level_sets[sets].which, kinds[i]);
        CHECK_INT(fake.level_sets[sets].level, 85);
        CHECK_INT(fake.level_sets[sets + 3].which, kinds[i]);
        CHECK_INT(fake.level_sets[sets + 3].level, 80);
    }
    /* From 83 down: 80. */
    fake.levels[DX_SOUND_RADIO] = 83;
    key(DX_UI_SELECT);
    key(DX_UI_SELECT);
    key(DX_UI_LEFT);
    CHECK_INT(fake.levels[DX_SOUND_RADIO], 80);
    /* The master never moved. */
    CHECK_INT(fake.volume, 80);
}

TEST(limits_refuse_with_one_deny)
{
    unsigned f = 0;
    int sets;
    int v;

    open_view();
    for (v = 100; v > 0; v -= 5) {
        send(DX_UI_LEFT, 1);
    }
    CHECK_INT(fake.levels[DX_SOUND_RADIO], 0);
    /* The steps store once, after the quiet time. */
    dx_service_autosave.tick(fake.ticks, &f);
    fake.ticks += DX_SAVE_QUIET_MS;
    dx_service_autosave.tick(fake.ticks, &f);
    CHECK_INT(fake.store_count, 1);
    fake.tone_count = 0;
    sets = fake.level_set_count;
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: radio level refused radio=0");
    CHECK_INT(tones_of(DENY_HZ), 1);
    CHECK_INT(send(DX_UI_LEFT, 1), DX_UI_HANDLED);
    CHECK_INT(tones_of(DENY_HZ), 1);
    key(DX_UI_DOWN);
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: radio level refused sfx=100");
    CHECK_INT(tones_of(DENY_HZ), 2);
    CHECK_INT(send(DX_UI_RIGHT, 1), DX_UI_HANDLED);
    CHECK_INT(tones_of(DENY_HZ), 2);
    CHECK_INT(fake.level_set_count, sets);
    /* No change counted: no second store. */
    fake.ticks += DX_SAVE_MAX_MS + 1;
    CHECK_INT(dx_service_autosave.tick(fake.ticks, &f), 0);
    CHECK_INT(fake.store_count, 1);
    draw();
    /* An empty bar: the outline only. */
    CHECK(strstr(fake.draws, "text 308 48 \"0\" fg none s1\nrect 108 51 176 10 dim outline\n"
                             "rect 0 64") != NULL);
}

TEST(held_left_in_the_view_never_changes_the_tab)
{
    dx_app_t app;
    int n;

    load_at(NULL, DX_ABI_1_6);
    shell_start(&app);
    push_key(DX_BTN_SELECT);
    for (n = 2 + fake_hold(DX_BTN_LEFT, 30); n > 0; n--) {
        step(&app);
    }
    CHECK_INT(app.tab, TAB_RADIO);
    CHECK(dx_radio_levels_is_open());
    CHECK_INT(fake.levels[DX_SOUND_RADIO], 0);
    CHECK(strstr(fake.log, "deitrix: tab=") == NULL);
}

/* ---- The sound of a step ------------------------------------------------- */

/* A step as the shell routes it: the screen, then the shell's sound. */
static unsigned step_as_shell(int type)
{
    dx_ui_event_t ev = { type, 0, 0 };
    unsigned r = radio->event(radio->st, &ev);

    dx_ui_sound_event(dx_ui_sound_for(&ev, r, 0), r, 0);
    return r;
}

TEST(a_step_plays_the_move_tone_as_its_kind)
{
    static const uint32_t kinds[] = { DX_SOUND_RADIO, DX_SOUND_SFX, DX_SOUND_RECORDINGS,
                                      DX_SOUND_GAMES };
    int i;

    open_view();
    for (i = 0; i < 4; i++) {
        int at, set;

        if (i > 0) {
            key(DX_UI_DOWN);
        }
        at = fake.audio_count;
        set = fake.level_set_count;
        step_as_shell(DX_UI_LEFT);
        /* One tone, after the set, and nothing from the shell. */
        CHECK_INT(fake.audio_count, at + 1);
        CHECK_INT(fake.level_set_count, set + 1);
        CHECK_INT(fake.level_sets[set].audio_at, at);
        CHECK_INT(fake.audio[at].op, FAKE_AU_TONE);
        CHECK_INT(fake.audio[at].n, MOVE_HZ);
        CHECK_INT(fake.audio[at].first, 12);
        CHECK_INT(fake.audio[at].kind, kinds[i]);
    }
}

TEST(no_tone_with_ui_sounds_off)
{
    open_view();
    fake.audio_count = 0;
    dx_ui_sound_enable(DX_UI_SOUND_OFF);
    CHECK_INT(step_as_shell(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED);
    CHECK_INT(fake.level_set_count, 1);
    CHECK_INT(fake.audio_count, 0);
}

TEST(a_tone_not_a_table_on_full)
{
    open_view();
    fake.audio_count = 0;
    fake.sysinfo.hw_flags = DX_HW_AUDIO | DX_HW_AUDIO_MID;
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    step_as_shell(DX_UI_LEFT);
    CHECK_INT(fake.audio_count, 1);
    CHECK_INT(fake.audio[0].op, FAKE_AU_TONE);
    CHECK_INT(fake.audio[0].n, MOVE_HZ);
    CHECK_INT(fake.audio[0].kind, DX_SOUND_RADIO);
    dx_ui_sound_enable(DX_UI_SOUND_ON);
}

TEST(no_tone_while_a_station_plays)
{
    load_at(SMALL, DX_ABI_1_6);
    /* Start the recorded station, then go to the Volume row. */
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    CHECK_INT(dx_radio_current(), 1);
    key(DX_UI_DOWN);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_LEVEL);
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    fake.audio_count = 0;
    fake.tone_count = 0;
    CHECK_INT(step_as_shell(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_SOUNDED);
    CHECK_INT(fake.levels[DX_SOUND_RECORDINGS], 95);
    CHECK_INT(fake.tone_count, 0);
    CHECK_INT(fake.audio_count, 0);
    CHECK_INT(dx_radio_current(), 1);
    dx_radio_stop();
}

/* ---- The dial -------------------------------------------------------------- */

TEST(the_dial_changes_the_tab_and_closes_the_view)
{
    dx_app_t app;

    load_at(NULL, DX_ABI_1_6);
    shell_start(&app);
    push_key(DX_BTN_SELECT);
    push_key(DX_BTN_LEFT);
    step(&app);
    step(&app);
    step(&app);
    step(&app);
    CHECK(dx_radio_levels_is_open());
    CHECK_INT(fake.levels[DX_SOUND_RADIO], 95);
    fake_push(DX_EV_DIAL, 0, 1);
    step(&app);
    CHECK(app.tab != TAB_RADIO);
    CHECK(!dx_radio_levels_is_open());
    fake_push(DX_EV_DIAL, 0, -1);
    step(&app);
    CHECK_INT(app.tab, TAB_RADIO);
    CHECK(!dx_radio_levels_is_open());
    CHECK_STR(radio->hint(radio->st), HINT_VOLUME);
    fake.log[0] = '\0';
    push_key(DX_BTN_UP);
    step(&app);
    step(&app);
    CHECK(strstr(fake.log, "deitrix: radio row=") != NULL ||
          strstr(fake.log, "deitrix: radio end row=") != NULL);
    CHECK_INT(fake.levels[DX_SOUND_RADIO], 95);
}

/* ---- Failed calls ---------------------------------------------------------- */

TEST(a_failed_read_keeps_the_view_closed)
{
    load_at(NULL, DX_ABI_1_6);
    fake.level_rc = DX_EIO;
    CHECK_INT(key(DX_UI_SELECT), DX_UI_IGNORED);
    CHECK(!dx_radio_levels_is_open());
    CHECK_STR(fake.log, "deitrix: radio idle\n");
    fake.level_rc = 0;
}

TEST(a_failed_set_keeps_the_row)
{
    uint32_t f = 0;

    open_view();
    fake.level_rc = DX_EIO;
    fake.tone_count = 0;
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: radio level failed radio rc=-5");
    CHECK_INT(tones_of(DENY_HZ), 1);
    CHECK_INT(send(DX_UI_LEFT, 1), DX_UI_HANDLED);
    CHECK_INT(tones_of(DENY_HZ), 1);
    CHECK_INT(tones_of(MOVE_HZ), 0);
    CHECK_INT(fake.level_set_count, 0);
    draw();
    CHECK(strstr(fake.draws, "text 292 48 \"100\" bg fg s1\n") != NULL);
    fake.ticks += DX_SAVE_MAX_MS + 1;
    CHECK_INT(dx_service_autosave.tick(fake.ticks, &f), 0);
    CHECK_INT(fake.level_calls, 4 + 2);
    fake.level_rc = 0;
}

TEST(zz_no_leaks)
{
    fake_reset();
    CHECK_INT(fake_live_blocks(), 0);
}
