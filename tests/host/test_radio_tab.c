/* The RADIO tab against the fake platform and the radio service. */
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

static const dx_screen_t *const radio = &dx_screen_radio;
static char text[DX_RADIO_LIMIT];

/* 1 a tune, 2 a looping track of two chunks, 3 a track played once,
 * 4 a track whose file is missing. */
static const char SMALL[] =
    "version = 1\n"
    "[station.1]\nname = Morning\nkind = tune\nfile = MORN.TUN\n"
    "[station.2]\nname = Hearth\nkind = pcm\nfile = HEARTH.PCM\n"
    "[station.3]\nname = Once\nkind = pcm\nfile = ONCE.PCM\nloop = no\n"
    "[station.4]\nname = Gone\nkind = pcm\nfile = GONE.PCM\n";

/* Loads `ini` (NULL: no file) as the card's RADIO.INI, with the files
 * of SMALL, and starts the radio as dx_main does. */
static void load_at(const char *ini, uint8_t volume)
{
    fake_reset();
    if (ini != NULL) {
        fake_add_text(INI_PATH, ini);
    }
    fake_add_text("/PIPBOY/DATA/RADIO/MORN.TUN", "version 1\nC4 D4 R/8\n");
    fake_add_fill("/PIPBOY/DATA/RADIO/HEARTH.PCM", 2 * DX_RADIO_CHUNK, 128);
    fake_add_fill("/PIPBOY/DATA/RADIO/ONCE.PCM", 2 * DX_RADIO_CHUNK + 500, 128);
    dx_ui_sound_block(0);
    dx_content_load();
    dx_content_load_saves(volume);
    dx_radio_attach(dx_content_radio());
    dx_service_radio.init(fake.ticks);
    dx_radio_screen_reset();
    fake.log[0] = '\0';
}

static void load(const char *ini)
{
    load_at(ini, 80);
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

/* The y of the row of text t, or -1. */
static int text_y(const char *t)
{
    const fake_text_t *f = fake_find_text(t);

    return f == NULL ? -1 : f->y;
}

static int deny_tones(void)
{
    int n = 0;
    int i;

    for (i = 0; i < fake.tone_count; i++) {
        n += fake.tones[i][0] == DENY_HZ;
    }
    return n;
}

static void check_hint(const char *want)
{
    const char *h = radio->hint(radio->st);

    CHECK_STR(h, want);
    CHECK(strlen(h) <= 40);
}

TEST(first_view_draw_list)
{
    load(SMALL);
    draw();
    CHECK_STR(fake.draws,
              "rect 0 24 320 16 fg fill\n"
              "text 12 24 \"Morning\" bg fg s1\n"
              "text 12 40 \"Hearth\" fg none s1\n"
              "text 12 56 \"Once\" fg none s1\n"
              "text 12 72 \"Gone\" dim none s1\n"
              "line 0 124 319 124 faint\n"
              "text 12 130 \"Radio off.\" dim none s1\n"
              "line 0 168 319 168 faint\n"
              "text 12 174 \"Volume\" fg none s1\n"
              "rect 108 177 176 10 dim outline\n"
              "rect 110 179 137 6 fg fill\n"
              "text 300 174 \"80\" fg none s1\n"
              "text 12 190 \"UI sounds\" fg none s1\n"
              "text 300 190 \"On\" fg none s1\n");
    CHECK(inside_body());
    check_hint("UP/DOWN: station  SELECT: play");
    /* Off at start: no line until what is shown changes. */
    CHECK_STR(fake.log, "");
}

TEST(start_stop_and_switch)
{
    dx_settings_t *set = dx_content_settings();
    uint32_t changes;

    load(SMALL);
    changes = set->changes;
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(fake.log, "deitrix: radio station=1\n");
    CHECK_INT(dx_radio_state(), DX_RADIO_TUNE);
    CHECK_INT(fake.seq_count, 1);
    CHECK_INT(set->last_station, 1);
    CHECK_INT(set->changes, changes + 1);
    draw();
    CHECK_STR(strstr(fake.draws, "rect 0 24"),
              "rect 0 24 320 16 fg fill\n"
              "text 268 24 \"ON AIR\" bg fg s1\n"
              "text 12 24 \"Morning\" bg fg s1\n"
              "text 12 40 \"Hearth\" fg none s1\n"
              "text 12 56 \"Once\" fg none s1\n"
              "text 12 72 \"Gone\" dim none s1\n"
              "line 0 124 319 124 faint\n"
              "text 12 130 \"Playing Morning\" fg none s1\n"
              "text 12 146 \"tune\" dim none s1\n"
              "line 0 168 319 168 faint\n"
              "text 12 174 \"Volume\" fg none s1\n"
              "rect 108 177 176 10 dim outline\n"
              "rect 110 179 137 6 fg fill\n"
              "text 300 174 \"80\" fg none s1\n"
              "text 12 190 \"UI sounds\" fg none s1\n"
              "text 300 190 \"On\" fg none s1\n");
    CHECK(inside_body());
    check_hint("UP/DOWN: station  SELECT: stop");
    /* A held SELECT does nothing more. */
    CHECK_INT(send(DX_UI_SELECT, 1), DX_UI_IGNORED);
    CHECK_INT(fake.seq_count, 1);
    /* Another station switches: the tune stops, the stream starts. */
    CHECK_INT(key(DX_UI_DOWN), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: radio row=1");
    check_hint("UP/DOWN: station  SELECT: play");
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: radio station=2");
    CHECK_INT(dx_radio_state(), DX_RADIO_PCM);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 1);
    CHECK_INT(set->last_station, 2);
    draw();
    CHECK_INT(text_y("ON AIR"), 40);
    CHECK(has_text("Playing Hearth"));
    CHECK(has_text("recording"));
    CHECK(!has_text("HEARTH"));
    CHECK(strstr(fake.log, "deitrix: radio shows Playing Hearth\n") != NULL);
    /* SELECT on the playing station stops it; the station stays picked. */
    changes = set->changes;
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: radio off");
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 2);
    CHECK_INT(set->last_station, 2);
    CHECK_INT(set->changes, changes);
    draw();
    CHECK(!has_text("ON AIR"));
    CHECK(has_text("Radio off."));
    CHECK_STR(last_log(), "deitrix: radio shows Off");
    /* UI tones come back once the radio stops. */
    fake.tone_count = 0;
    dx_ui_sound(DX_SND_MOVE);
    CHECK_INT(fake.tone_count, 1);
}

/* Through the app shell with the radio service: one step, then the
 * clock moves to the wait's end, as the board would. */
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

TEST(block_follows_the_track_and_its_wrap)
{
    dx_app_t app;
    int i;

    load(SMALL);
    shell_start(&app);
    push_key(DX_BTN_DOWN);
    push_key(DX_BTN_SELECT);
    for (i = 0; i < 4; i++) {
        step(&app);
    }
    CHECK_INT(dx_radio_current(), 1);
    CHECK(fake_find_text("Playing Hearth") != NULL);
    CHECK(fake_find_text("recording") != NULL);
    CHECK(fake_find_text("track 1 of 1") == NULL);
    /* The track is two chunks long: 6 s of play wrap it three times.
     * Nothing shown changes, so no frame. */
    fake_clear_draws();
    fake.presents = 0;
    for (i = 0; i < 24; i++) {
        step(&app);
    }
    CHECK(fake.au_bytes >= 6 * DX_RADIO_CHUNK);
    CHECK_INT(dx_radio_current(), 1);
    CHECK_INT(fake.presents, 0);
    CHECK_INT(dx_radio_underruns(), 0);
    /* The track played once ends: the mark and the block go, in the
     * step that saw it end. */
    push_key(DX_BTN_DOWN);
    push_key(DX_BTN_SELECT);
    for (i = 0; i < 4; i++) {
        step(&app);
    }
    CHECK(fake_find_text("Playing Once") != NULL);
    for (i = 0; i < 40 && dx_radio_state() != DX_RADIO_OFF; i++) {
        fake_clear_draws();
        step(&app);
    }
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK(fake_find_text("Radio off.") != NULL);
    CHECK(fake_find_text("ON AIR") == NULL);
    CHECK(strstr(fake.log, "deitrix: radio shows Playing Once\n"
                           "deitrix: radio shows Off\n") != NULL);
    CHECK(fake_find_text("UP/DOWN: station  SELECT: play") != NULL);
}

TEST(station_with_nothing_to_play)
{
    load(SMALL);
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    key(DX_UI_DOWN);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(fake.log, "deitrix: radio row=1\ndeitrix: radio row=2\ndeitrix: radio row=3\n"
                        "deitrix: radio station=4\ndeitrix: radio cannot play\n");
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(dx_content_settings()->last_station, 4);
    draw();
    CHECK_INT(text_y("Station has nothing to play."), 130);
    CHECK_INT(text_y("GONE.PCM missing"), 146);
    CHECK(!has_text("ON AIR"));
    CHECK(inside_body());
    CHECK_STR(last_log(), "deitrix: radio shows Cannot play Gone");
    check_hint("UP/DOWN: station  SELECT: play");
    /* A file that breaks while it streams says the same. */
    key(DX_UI_UP);
    key(DX_UI_UP);
    key(DX_UI_SELECT);
    CHECK_INT(dx_radio_state(), DX_RADIO_PCM);
    fake.read_fail = 1;
    while (dx_radio_state() == DX_RADIO_PCM && fake.ticks < 10000) {
        unsigned flags = 0;

        fake.ticks += DX_RADIO_POLL_MS;
        dx_service_radio.tick(fake.ticks, &flags);
    }
    fake.read_fail = 0;
    draw();
    CHECK(has_text("Station has nothing to play."));
    CHECK(has_text("HEARTH.PCM did not play"));
    CHECK_STR(last_log(), "deitrix: radio shows Cannot play Hearth");
    /* A start that works clears it. */
    key(DX_UI_UP);
    key(DX_UI_SELECT);
    draw();
    CHECK(has_text("Playing Morning"));
    dx_radio_stop();
}

TEST(volume_to_both_limits)
{
    dx_settings_t *set = dx_content_settings();
    uint32_t changes;
    int v;

    load_at(SMALL, 83);
    CHECK_INT(set->volume, 83);
    for (v = 0; v < 4; v++) {
        key(DX_UI_DOWN);
    }
    CHECK_STR(last_log(), "deitrix: radio row=4");
    check_hint("LEFT/RIGHT: volume  UP/DOWN: move");
    draw();
    /* The row is inverse, the station list has no selection. */
    CHECK(strstr(fake.draws, "text 12 24 \"Morning\" fg none s1\n") != NULL);
    CHECK(strstr(fake.draws, "rect 0 174 320 16 fg fill\n"
                             "text 12 174 \"Volume\" bg fg s1\n"
                             "rect 108 177 176 10 bg outline\n") != NULL);
    /* Off the grid: the next multiple of 10 each way. */
    changes = set->changes;
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(set->volume, 90);
    CHECK_INT(fake.volume, 90);
    CHECK_STR(last_log(), "deitrix: radio volume=90");
    CHECK_INT(send(DX_UI_RIGHT, 1), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(set->volume, 100);
    CHECK_INT(set->changes, changes + 2);
    /* At the top: refused, no change counted, a deny tone for a press
     * and none for a repeat. */
    fake.tone_count = 0;
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: radio refused volume=100");
    CHECK_INT(deny_tones(), 1);
    CHECK_INT(send(DX_UI_RIGHT, 1), DX_UI_HANDLED);
    CHECK_INT(deny_tones(), 1);
    CHECK_INT(set->changes, changes + 2);
    draw();
    CHECK(has_text("100"));
    CHECK(strstr(fake.draws, "rect 110 179 172 6 bg fill\n") != NULL);
    CHECK(inside_body());
    for (v = 90; v >= 0; v -= 10) {
        CHECK_INT(send(DX_UI_LEFT, 1), DX_UI_HANDLED | DX_UI_DIRTY);
        CHECK_INT(set->volume, v);
        CHECK_INT(fake.volume, (uint32_t)v);
    }
    CHECK_INT(set->changes, changes + 12);
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED);
    CHECK_STR(last_log(), "deitrix: radio refused volume=0");
    CHECK_INT(deny_tones(), 2);
    CHECK_INT(set->changes, changes + 12);
    draw();
    /* An empty bar: the outline only. */
    CHECK(strstr(fake.draws, "rect 108 177 176 10 bg outline\ntext 308 174 \"0\" bg fg s1\n") != NULL);
    /* SELECT and Back on the row do nothing. */
    CHECK_INT(key(DX_UI_SELECT), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_BACK), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: radio idle");
    CHECK_INT(set->changes, changes + 12);
}

/* A held SELECT on a station starts it once, and a held SELECT on the
 * station on air stops it once. Held BACK does nothing. */
TEST(held_select_starts_and_stops_once)
{
    dx_settings_t *set = dx_content_settings();
    uint32_t changes;
    int i;

    load(SMALL);
    changes = set->changes;
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    for (i = 0; i < 5; i++) {
        CHECK_INT(send(DX_UI_SELECT, 1), DX_UI_IGNORED);
        CHECK_INT(send(DX_UI_BACK, 1), DX_UI_IGNORED);
    }
    CHECK_INT(dx_radio_current(), 0);
    CHECK_INT(fake.seq_count, 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 0);
    CHECK_INT(set->changes, changes + 1);
    CHECK(strstr(fake.log, "deitrix: radio station=1\n") == fake.log);
    CHECK(strstr(fake.log + 1, "deitrix: radio station=") == NULL);
    CHECK_STR(last_log(), "deitrix: radio idle");
    fake.log[0] = '\0';
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    for (i = 0; i < 5; i++) {
        CHECK_INT(send(DX_UI_SELECT, 1), DX_UI_IGNORED);
    }
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(fake.seq_count, 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
    CHECK(strstr(fake.log, "deitrix: radio off\n") == fake.log);
    CHECK(strstr(fake.log, "station=") == NULL);
}

TEST(ui_sounds_toggle)
{
    dx_settings_t *set = dx_content_settings();
    uint32_t changes;
    int i;

    load(SMALL);
    for (i = 0; i < 5; i++) {
        key(DX_UI_DOWN);
    }
    CHECK_INT(key(DX_UI_DOWN), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: radio end row=5");
    check_hint("SELECT: UI sounds on/off  UP/DOWN: move");
    changes = set->changes;
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_STR(last_log(), "deitrix: radio sound=0");
    CHECK_INT(set->ui_sound, 0);
    CHECK_INT(set->changes, changes + 1);
    fake.tone_count = 0;
    dx_ui_sound(DX_SND_MOVE);
    CHECK_INT(fake.tone_count, 0);
    draw();
    CHECK(strstr(fake.draws, "rect 0 190 320 16 fg fill\n"
                             "text 12 190 \"UI sounds\" bg fg s1\n"
                             "text 292 190 \"Off\" bg fg s1\n") != NULL);
    /* A held key does not toggle again, and never changes the tab. */
    CHECK_INT(send(DX_UI_SELECT, 1), DX_UI_HANDLED);
    CHECK_INT(send(DX_UI_RIGHT, 1), DX_UI_HANDLED);
    CHECK_INT(send(DX_UI_LEFT, 1), DX_UI_HANDLED);
    CHECK_INT(send(DX_UI_BACK, 1), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: radio idle");
    CHECK_INT(set->ui_sound, 0);
    CHECK_INT(set->changes, changes + 1);
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(set->ui_sound, 1);
    dx_ui_sound(DX_SND_MOVE);
    CHECK_INT(fake.tone_count, 1);
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(set->ui_sound, 0);
    CHECK_INT(set->changes, changes + 3);
    dx_ui_sound_enable(1);
}

TEST(left_right_reach_the_shell_on_a_station)
{
    dx_app_t app;

    load(SMALL);
    shell_start(&app);
    CHECK_INT(app.tab, TAB_RADIO);
    /* On a station: the shell changes the tab, and the tab logs nothing. */
    fake.log[0] = '\0';
    CHECK_INT(key(DX_UI_RIGHT), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_LEFT), DX_UI_IGNORED);
    CHECK_STR(fake.log, "");
    push_key(DX_BTN_RIGHT);
    step(&app);
    step(&app);
    CHECK_INT(app.tab, 0);
    push_key(DX_BTN_LEFT);
    step(&app);
    step(&app);
    CHECK_INT(app.tab, TAB_RADIO);
    /* On a setting: the tab stays. */
    dx_radio_screen_reset();
    dx_settings_set_station(dx_content_settings(), 4);
    fake.log[0] = '\0';
    push_key(DX_BTN_DOWN);
    push_key(DX_BTN_LEFT);
    push_key(DX_BTN_RIGHT);
    push_key(DX_BTN_DOWN);
    push_key(DX_BTN_LEFT);
    push_key(DX_BTN_RIGHT);
    push_key(DX_BTN_BACK);
    {
        int i;

        for (i = 0; i < 16; i++) {
            step(&app);
        }
    }
    CHECK_INT(app.tab, TAB_RADIO);
    CHECK(strstr(fake.log, "deitrix: radio row=4\ndeitrix: radio volume=70\n"
                           "deitrix: radio volume=80\ndeitrix: radio row=5\n"
                           "deitrix: radio sound=0\ndeitrix: radio sound=1\n"
                           "deitrix: radio idle\n") != NULL);
    CHECK(strstr(fake.log, "tab=") == NULL);
    /* Leaving the tab does not stop the radio. */
    dx_settings_set_station(dx_content_settings(), 2);
    dx_radio_screen_reset();
    push_key(DX_BTN_SELECT);
    fake_push(DX_EV_DIAL, 0, 1);
    step(&app);
    step(&app);
    step(&app);
    CHECK_INT(app.tab, 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_PCM);
    dx_radio_stop();
}

TEST(cursor_starts_at_the_last_station)
{
    load(SMALL);
    dx_settings_set_station(dx_content_settings(), 3);
    dx_radio_screen_reset();
    draw();
    CHECK(strstr(fake.draws, "text 12 56 \"Once\" bg fg s1\n") != NULL);
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF); /* picked, not playing */
    /* A number with no station: the first row. */
    dx_settings_set_station(dx_content_settings(), 6);
    dx_radio_screen_reset();
    draw();
    CHECK(strstr(fake.draws, "text 12 24 \"Morning\" bg fg s1\n") != NULL);
}

TEST(no_file)
{
    dx_settings_t *set = dx_content_settings();
    int calls;

    load(NULL);
    calls = fake.audio_count;
    draw();
    CHECK(has_text("No stations on the card."));
    CHECK(has_text("RADIO.INI missing"));
    CHECK(has_text("Radio off."));
    CHECK(inside_body());
    /* The cursor starts on the volume; the settings still work. */
    check_hint("LEFT/RIGHT: volume  UP/DOWN: move");
    CHECK_INT(key(DX_UI_UP), DX_UI_IGNORED);
    CHECK_STR(last_log(), "deitrix: radio end row=0");
    CHECK_INT(key(DX_UI_SELECT), DX_UI_IGNORED);
    CHECK_INT(key(DX_UI_LEFT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(set->volume, 70);
    CHECK_INT(key(DX_UI_DOWN), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(key(DX_UI_SELECT), DX_UI_HANDLED | DX_UI_DIRTY);
    CHECK_INT(set->ui_sound, 0);
    CHECK_INT(fake.audio_count, calls + 1); /* the volume call only */
    dx_ui_sound_enable(1);
}

TEST(broken_file_and_no_station)
{
    load("version = 1\n[station.1]\nname = A\nkind = tune\nfile = A.TUN\nbad line\n");
    draw();
    CHECK(has_text("No stations on the card."));
    CHECK(has_text("RADIO.INI line 6"));
    CHECK(inside_body());
    load("version = 1\n");
    draw();
    CHECK(has_text("No stations on the card."));
    CHECK(!has_text("RADIO.INI missing"));
    CHECK(inside_body());
}

/* Six stations with names of 20 wide letters, each a missing file but
 * the first, a tune. */
TEST(limits_stay_inside_the_body)
{
    int n = 0;
    int i;

    n += tfmt(text + n, sizeof(text) - (size_t)n, "version = 1\n");
    for (i = 1; i <= 6; i++) {
        n += tfmt(text + n, sizeof(text) - (size_t)n,
                  "[station.%d]\nname = WWWWWWWWWWWWWWWWWWW%d\nkind = %s\nfile = %s\n", i, i,
                  i == 1 ? "tune" : "pcm", i == 1 ? "MORN.TUN" : "ABCDEFGH.PCM");
    }
    load(text);
    CHECK_INT(dx_content_radio()->count, 6);
    key(DX_UI_SELECT);
    draw();
    CHECK(inside_body());
    CHECK(has_text("WWWWWWWWWWWWWWWWWWW1"));
    CHECK(has_text("ON AIR"));
    CHECK_INT(text_y("WWWWWWWWWWWWWWWWWWW6"), 104);
    CHECK(text_y("WWWWWWWWWWWWWWWWWWW6") + DX_ROW_H <= text_y("Playing WWWWWWWWWWWWWWWWWWW1"));
    for (i = 1; i < 6; i++) {
        key(DX_UI_DOWN);
        key(DX_UI_SELECT);
        draw();
        CHECK(inside_body());
        CHECK(has_text("ABCDEFGH.PCM missing"));
    }
    key(DX_UI_DOWN);
    draw();
    CHECK(inside_body());
    key(DX_UI_DOWN);
    draw();
    CHECK(inside_body());
    CHECK(strstr(fake.log, "deitrix: radio shows Cannot play WWWWWWWWWWWWWWWWWWW6\n") != NULL);
}
