/* The weep and the tables: when they play, and the order of their calls
 * on the fake audio. */
#include "rc_test.h"

#include <string.h>

#include <robco/crc32.h>

#include "app.h"
#include "fake_platform.h"
#include "screens/screens.h"
#include "services/services.h"
#include "services/sfx_table.h"
#include "services/weep_table.h"
#include "ui/ui.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

#define FLAGS_OK (DX_HW_AUDIO | DX_HW_AUDIO_MID)

/* A kernel that lets the weep play: ABI 1.5, audio resting mid. */
static void setup(void)
{
    fake_reset();
    dx_ui_sound_enable(1);
    dx_ui_sound_block(0);
    dx_radio_stop();
    fake.sysinfo.abi_version = DX_ABI_1_5;
    fake.sysinfo.hw_flags = FLAGS_OK | DX_HW_RTC;
}

TEST(weep_allowed_every_case)
{
    /* Every input on: it plays. */
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, FLAGS_OK), 1);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, 0x00010006u, FLAGS_OK | DX_HW_TAPE), 1);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, 0x00020000u, FLAGS_OK), 1);
    /* Each one off on its own: silent. */
    CHECK_INT(dx_sfx_weep_allowed(0, 0, DX_ABI_1_5, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 1, DX_ABI_1_5, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_4, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_3, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, 0, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, DX_HW_AUDIO), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, DX_HW_AUDIO_MID), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, 0), 0);
    /* Every input off. */
    CHECK_INT(dx_sfx_weep_allowed(0, 1, 0, 0), 0);
}

TEST(weep_sets_the_rate_plays_and_restores)
{
    setup();
    CHECK_INT(dx_sfx_weep(), 1);
    CHECK_INT(fake.audio_count, 3);
    CHECK_INT(fake.audio[0].op, FAKE_AU_RATE);
    CHECK_INT(fake.audio[0].n, 37500);
    CHECK_INT(fake.audio[1].op, FAKE_AU_SAMPLE);
    CHECK_INT(fake.audio[1].n, 5250);
    CHECK_INT(fake.audio[1].first, 128);
    CHECK_INT(fake.audio[2].op, FAKE_AU_RATE);
    CHECK_INT(fake.audio[2].n, 8000);
    /* The play took the high rate, and the rate is back to 8000. */
    CHECK_INT(fake.au_rate, 37500);
    CHECK_INT(fake.rate, 8000);
    CHECK_INT(fake.au_bytes, DX_WEEP_LEN);
    CHECK_INT(fake.au_crc, crc32(dx_weep, DX_WEEP_LEN));
}

TEST(weep_restores_whatever_rate_was_set)
{
    setup();
    fake.rate = DX_RATE_HI;
    CHECK_INT(dx_sfx_weep(), 1);
    CHECK_INT(fake.audio[2].n, 37500);
    CHECK_INT(fake.rate, 37500);
}

TEST(weep_table_shape)
{
    CHECK_INT(DX_WEEP_LEN, 5250);
    CHECK_INT(DX_WEEP_RATE, 37500);
    CHECK_INT(dx_weep[0], 128);
    CHECK_INT(dx_weep[DX_WEEP_LEN - 1], 128);
}

TEST(weep_silent_cases)
{
    /* UI sounds off. */
    setup();
    dx_ui_sound_enable(0);
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    dx_ui_sound_enable(1);
    /* An older kernel. */
    setup();
    fake.sysinfo.abi_version = DX_ABI_1_4;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* No audio driver. */
    setup();
    fake.sysinfo.hw_flags = DX_HW_AUDIO_MID;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* The output rests low. */
    setup();
    fake.sysinfo.hw_flags = DX_HW_AUDIO;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* sys_info fails. */
    setup();
    fake.sysinfo_fail = 1;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* sample_rate refuses: no sample at the wrong rate. */
    setup();
    fake.rate_result = DX_EINVAL;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
}

TEST(weep_silent_while_the_radio_plays)
{
    static const char ini[] = "version = 1\n[station.1]\nname = Tune\nkind = tune\nfile = T.TUN\n";
    static const char tune[] = "version 1\nC4 D4\n";
    static dx_radio_t radio;

    setup();
    dx_radio_load(&radio, (const uint8_t *)ini, (int32_t)strlen(ini));
    dx_radio_set_tune(&radio, 0, (const uint8_t *)tune, (int32_t)strlen(tune));
    dx_radio_attach(&radio);
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    CHECK(dx_radio_state() != DX_RADIO_OFF);
    fake.audio_count = 0;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    dx_radio_stop();
    CHECK_INT(dx_sfx_weep(), 1);
}

TEST(weep_once_after_the_first_frame)
{
    static dx_app_t app;
    const char *ready;

    setup();
    dx_app_init(&app, dx_rom_screens, dx_rom_screen_count);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
    dx_app_step(&app);
    ready = strstr(fake.log, "deitrix: ready\n");
    CHECK(ready != NULL);
    CHECK_EQ(fake.presents, 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_RATE), 2);
    /* Another frame plays nothing more. */
    app.dirty = 1;
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 2);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 1);
    dx_radio_stop();
}

TEST(no_weep_on_the_default_fake)
{
    static dx_app_t app;

    /* The fake's kernel reports nothing: the frame comes with no sound. */
    fake_reset();
    dx_app_init(&app, dx_rom_screens, dx_rom_screen_count);
    dx_app_step(&app);
    CHECK_INT(fake.audio_count, 0);
}

/* ---- The tables of src/services/sfx_table.c ------------------------------ */

TEST(sfx_header_matches_the_tables)
{
    static const struct {
        const uint8_t *data;
        uint32_t len;
        uint32_t crc;
    } t[] = {
        { dx_sfx_scroll, DX_SFX_SCROLL_LEN, DX_SFX_SCROLL_CRC32 },
        { dx_sfx_select, DX_SFX_SELECT_LEN, DX_SFX_SELECT_CRC32 },
        { dx_sfx_mode, DX_SFX_MODE_LEN, DX_SFX_MODE_CRC32 },
        { dx_sfx_tab, DX_SFX_TAB_LEN, DX_SFX_TAB_CRC32 },
        { dx_sfx_highlight, DX_SFX_HIGHLIGHT_LEN, DX_SFX_HIGHLIGHT_CRC32 },
        { dx_sfx_tuner, DX_SFX_TUNER_LEN, DX_SFX_TUNER_CRC32 },
        { dx_sfx_tape_start, DX_SFX_TAPE_START_LEN, DX_SFX_TAPE_START_CRC32 },
        { dx_sfx_tape_stop, DX_SFX_TAPE_STOP_LEN, DX_SFX_TAPE_STOP_CRC32 },
        { dx_sfx_hum_1, DX_SFX_HUM_1_LEN, DX_SFX_HUM_1_CRC32 },
        { dx_sfx_hum_2, DX_SFX_HUM_2_LEN, DX_SFX_HUM_2_CRC32 },
    };
    size_t i;

    CHECK_INT(DX_SFX_RATE_HZ, DX_RATE_HI);
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        CHECK(t[i].len >= 2 && t[i].len <= DX_SAMPLE_MAX);
        CHECK_INT(crc32(t[i].data, t[i].len), t[i].crc);
    }
}

TEST(sfx_loudest_one_shot_over_the_loop_fits)
{
    static const unsigned shots[] = {
        DX_SFX_SCROLL_PEAK, DX_SFX_SELECT_PEAK, DX_SFX_MODE_PEAK, DX_SFX_TAB_PEAK,
        DX_SFX_HIGHLIGHT_PEAK, DX_SFX_TUNER_PEAK, DX_SFX_TAPE_START_PEAK, DX_SFX_TAPE_STOP_PEAK,
    };
    unsigned top = 0;
    size_t i;

    for (i = 0; i < sizeof(shots) / sizeof(shots[0]); i++) {
        top = shots[i] > top ? shots[i] : top;
    }
    CHECK_INT(DX_SFX_HUM_LOOP, 1);
    CHECK(top + DX_SFX_HUM_PEAK <= 127);
}

TEST(sfx_play_sets_the_rate_plays_and_restores)
{
    setup();
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 1);
    CHECK_INT(fake.audio_count, 3);
    CHECK_INT(fake.audio[0].op, FAKE_AU_RATE);
    CHECK_INT(fake.audio[0].n, 37500);
    CHECK_INT(fake.audio[1].op, FAKE_AU_SAMPLE);
    CHECK_INT(fake.audio[1].n, DX_SFX_SCROLL_LEN);
    CHECK_INT(fake.audio[2].op, FAKE_AU_RATE);
    CHECK_INT(fake.audio[2].n, 8000);
    CHECK_INT(fake.au_rate, 37500);
    CHECK_INT(fake.rate, 8000);
    CHECK_INT(fake.au_crc, DX_SFX_SCROLL_CRC32);
    setup();
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SELECT), 1);
    CHECK_INT(fake.au_crc, DX_SFX_SELECT_CRC32);
    CHECK_INT(fake.rate, 8000);
}

TEST(sfx_play_silent_cases)
{
    setup();
    CHECK_INT(dx_sfx_play(-1), 0);
    CHECK_INT(dx_sfx_play(DX_SFX_COUNT), 0);
    CHECK_INT(fake.audio_count, 0);
    setup();
    fake.sysinfo.abi_version = DX_ABI_1_4;
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 0);
    setup();
    fake.sysinfo.hw_flags = DX_HW_AUDIO;
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 0);
    CHECK_INT(dx_sfx_full(), 0);
    CHECK_INT(fake.audio_count, 0);
    setup();
    fake.rate_result = DX_EINVAL;
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 0);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
}

TEST(ui_sound_full_plays_tables_where_it_may)
{
    setup();
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    /* A row move and a select play their tables. */
    dx_ui_sound_event(DX_SND_MOVE, DX_UI_ROW_MOVED, 0);
    CHECK_INT(fake.au_crc, DX_SFX_SCROLL_CRC32);
    CHECK_INT(fake.tone_count, 0);
    setup();
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    dx_ui_sound_event(DX_SND_SELECT, 0, 0);
    CHECK_INT(fake.au_crc, DX_SFX_SELECT_CRC32);
    CHECK_INT(fake.tone_count, 0);
    /* A mode change, a sub-tab move and a back play their tables. */
    {
        static const struct {
            int snd;
            unsigned result;
            int changed;
            uint32_t crc;
        } cases[] = {
            { DX_SND_MOVE, DX_UI_ROW_MOVED, 1, DX_SFX_MODE_CRC32 },
            { DX_SND_MOVE, DX_UI_TAB_MOVED | DX_UI_ROW_MOVED, 0, DX_SFX_TAB_CRC32 },
            { DX_SND_BACK, DX_UI_HANDLED, 0, DX_SFX_HIGHLIGHT_CRC32 },
        };
        int i;

        for (i = 0; i < 3; i++) {
            setup();
            dx_ui_sound_enable(DX_UI_SOUND_FULL);
            dx_ui_sound_event(cases[i].snd, cases[i].result, cases[i].changed);
            CHECK_INT(fake.au_crc, cases[i].crc);
            CHECK_INT(fake.tone_count, 0);
        }
    }
    /* Deny keeps its tone, flags or not. */
    setup();
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    dx_ui_sound_event(DX_SND_DENY, DX_UI_TAB_MOVED | DX_UI_ROW_MOVED, 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
    CHECK_INT(fake.tone_count, 1);
    CHECK_INT(fake.tones[0][0], 180);
    /* A move that is not a row move, a value change or a volume step,
     * keeps the move tone. */
    setup();
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    dx_ui_sound_event(DX_SND_MOVE, DX_UI_HANDLED | DX_UI_DIRTY, 0);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
    CHECK_INT(fake.tone_count, 1);
    CHECK_INT(fake.tones[0][0], 1400);
    /* Blocked by the radio: nothing. */
    setup();
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    dx_ui_sound_block(1);
    dx_ui_sound_event(DX_SND_MOVE, DX_UI_ROW_MOVED, 0);
    CHECK_INT(fake.audio_count, 0);
    CHECK_INT(fake.tone_count, 0);
    dx_ui_sound_block(0);
    dx_ui_sound_enable(DX_UI_SOUND_ON);
}

/* The events of the shell, as dx_ui_sound_event gets them, and the
 * tone each plays on ON. */
static void play_every_event(void)
{
    dx_ui_sound_event(DX_SND_MOVE, DX_UI_ROW_MOVED, 0);
    dx_ui_sound_event(DX_SND_SELECT, 0, 0);
    dx_ui_sound_event(DX_SND_MOVE, DX_UI_IGNORED, 1);
    dx_ui_sound_event(DX_SND_MOVE, DX_UI_HANDLED | DX_UI_DIRTY | DX_UI_TAB_MOVED, 0);
    dx_ui_sound_event(DX_SND_BACK, DX_UI_HANDLED, 0);
    dx_ui_sound_event(DX_SND_DENY, DX_UI_IGNORED, 0);
}

static void check_every_tone(void)
{
    static const int hz[6] = { 1400, 1900, 1400, 1400, 900, 180 };
    int i;

    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
    CHECK_INT(fake_audio_ops(FAKE_AU_RATE), 0);
    CHECK_INT(fake.tone_count, 6);
    for (i = 0; i < 6; i++) {
        CHECK_INT(fake.tones[i][0], hz[i]);
    }
}

TEST(ui_sound_full_without_the_bit_plays_tones)
{
    setup();
    fake.sysinfo.hw_flags = DX_HW_AUDIO;
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    CHECK_INT(dx_ui_sound_enabled(), 1);
    play_every_event();
    check_every_tone();
    /* ON plays tones even with the bit. */
    setup();
    dx_ui_sound_enable(DX_UI_SOUND_ON);
    play_every_event();
    check_every_tone();
    /* OFF plays nothing. */
    setup();
    dx_ui_sound_enable(DX_UI_SOUND_OFF);
    play_every_event();
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
    CHECK_INT(fake.tone_count, 0);
    dx_ui_sound_enable(DX_UI_SOUND_ON);
}

TEST(ui_sound_row_values)
{
    /* With the bit: Off, On, Full and round. */
    CHECK_INT(dx_ui_sound_next(DX_UI_SOUND_OFF, 1), DX_UI_SOUND_ON);
    CHECK_INT(dx_ui_sound_next(DX_UI_SOUND_ON, 1), DX_UI_SOUND_FULL);
    CHECK_INT(dx_ui_sound_next(DX_UI_SOUND_FULL, 1), DX_UI_SOUND_OFF);
    CHECK_STR(dx_ui_sound_label(DX_UI_SOUND_OFF, 1), "Off");
    CHECK_STR(dx_ui_sound_label(DX_UI_SOUND_ON, 1), "On");
    CHECK_STR(dx_ui_sound_label(DX_UI_SOUND_FULL, 1), "Full");
    /* Without it: Off and On. A saved Full reads On and turns Off. */
    CHECK_INT(dx_ui_sound_next(DX_UI_SOUND_OFF, 0), DX_UI_SOUND_ON);
    CHECK_INT(dx_ui_sound_next(DX_UI_SOUND_ON, 0), DX_UI_SOUND_OFF);
    CHECK_INT(dx_ui_sound_next(DX_UI_SOUND_FULL, 0), DX_UI_SOUND_OFF);
    CHECK_STR(dx_ui_sound_label(DX_UI_SOUND_OFF, 0), "Off");
    CHECK_STR(dx_ui_sound_label(DX_UI_SOUND_ON, 0), "On");
    CHECK_STR(dx_ui_sound_label(DX_UI_SOUND_FULL, 0), "On");
}

/* ---- The wait before a tape -------------------------------------------- */

TEST(sfx_wait_more_rule)
{
    /* The status turns 0: the wait ends at once. */
    CHECK_INT(dx_sfx_wait_more(1, 1, 0, 0), 1);
    CHECK_INT(dx_sfx_wait_more(1, 0, 0, 0), 0);
    CHECK_INT(dx_sfx_wait_more(1, 0, 120, 0), 0);
    /* The limit: 499 ms waits on, 500 ms ends it. */
    CHECK_INT(dx_sfx_wait_more(1, 1, DX_SFX_DRAIN_MS - 1, 0), 1);
    CHECK_INT(dx_sfx_wait_more(1, 1, DX_SFX_DRAIN_MS, 0), 0);
    CHECK_INT(dx_sfx_wait_more(1, 1, DX_SFX_DRAIN_MS + 1, 0), 0);
    /* No sound at all: no wait, busy or not. */
    CHECK_INT(dx_sfx_wait_more(0, 1, 0, 0), 0);
    CHECK_INT(dx_sfx_wait_more(0, 0, 0, 0), 0);
    CHECK_INT(DX_SFX_DRAIN_MS, 500);
    /* The round cap ends a wait on a clock that stands still. */
    CHECK_INT(dx_sfx_wait_more(1, 1, 0, DX_SFX_DRAIN_ROUNDS - 1), 1);
    CHECK_INT(dx_sfx_wait_more(1, 1, 0, DX_SFX_DRAIN_ROUNDS), 0);
    CHECK_INT(DX_SFX_DRAIN_ROUNDS, 5000);
}

TEST(sfx_drain_waits_for_the_end)
{
    uint32_t t0;

    setup();
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    fake.tick_step = 10;
    t0 = fake.ticks;
    CHECK_INT(dx_ui_sound_play(DX_SFX_ID_TAPE_START), 1);
    CHECK_INT(fake.au_crc, DX_SFX_TAPE_START_CRC32);
    dx_sfx_drain(1);
    /* 16269 bytes at 37500 Hz: 433 ms, then the wait ends. */
    CHECK_INT(dx_plat_audio_status(), 0);
    CHECK(fake.ticks - t0 >= DX_SFX_TAPE_START_LEN * 1000u / 37500u);
    CHECK(fake.ticks - t0 < DX_SFX_DRAIN_MS);
    CHECK(fake.waits > 0);
    /* Every wait is short, so the end is close. */
    CHECK(fake.last_timeout <= 10);
    dx_ui_sound_enable(DX_UI_SOUND_ON);
}

TEST(sfx_drain_stops_at_the_limit_and_without_a_sound)
{
    static const dx_note_t long_note[] = { { 440, 2000 } };
    uint32_t t0;

    /* A sound longer than the limit: the wait gives up at 500 ms. */
    setup();
    fake.tick_step = 10;
    t0 = fake.ticks;
    CHECK_INT(dx_plat_play_seq(long_note, 1), 0);
    dx_sfx_drain(1);
    CHECK_INT(dx_plat_audio_status(), 1);
    CHECK_INT(fake.ticks - t0, DX_SFX_DRAIN_MS);
    /* No sound started: no wait at all, even with the voice busy. */
    setup();
    fake.tick_step = 10;
    CHECK_INT(dx_plat_play_seq(long_note, 1), 0);
    dx_sfx_drain(0);
    CHECK_INT(fake.waits, 0);
}

TEST(sfx_drain_counts_time_not_rounds)
{
    static const dx_note_t long_note[] = { { 440, 2000 } };
    uint32_t t0;
    int i;

    /* A key burst: every round returns at once after 1 ms. The wait
     * still lasts 500 ms by the clock, far past 100 rounds. */
    setup();
    fake.tick_step = 1;
    for (i = 0; i < FAKE_MAX_EVENTS; i++) {
        fake_push(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_DOWN);
    }
    t0 = fake.ticks;
    CHECK_INT(dx_plat_play_seq(long_note, 1), 0);
    dx_sfx_drain(1);
    CHECK_INT(fake.ticks - t0, DX_SFX_DRAIN_MS);
    CHECK_INT(fake.waits, DX_SFX_DRAIN_MS);
    /* The status turns 0 within a burst: the wait ends then. */
    setup();
    fake.tick_step = 1;
    for (i = 0; i < FAKE_MAX_EVENTS; i++) {
        fake_push(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_DOWN);
    }
    dx_ui_sound_enable(DX_UI_SOUND_FULL);
    t0 = fake.ticks;
    CHECK_INT(dx_ui_sound_play(DX_SFX_ID_TAPE_START), 1);
    dx_sfx_drain(1);
    CHECK_INT(dx_plat_audio_status(), 0);
    CHECK(fake.ticks - t0 < DX_SFX_DRAIN_MS);
    dx_ui_sound_enable(DX_UI_SOUND_ON);
    /* The clock wraps in the wait: still 500 ms. */
    setup();
    fake.tick_step = 10;
    fake.ticks = 0xFFFFFF00u;
    t0 = fake.ticks;
    CHECK_INT(dx_plat_play_seq(long_note, 1), 0);
    dx_sfx_drain(1);
    CHECK_INT(fake.ticks - t0, DX_SFX_DRAIN_MS);
    /* A clock that stands still: the round cap ends the wait. */
    setup();
    fake.tick_step = 0;
    CHECK_INT(dx_plat_play_seq(long_note, 1), 0);
    dx_sfx_drain(1);
    CHECK_INT(fake.waits, DX_SFX_DRAIN_ROUNDS);
}

/* ---- The tuning state ---------------------------------------------------- */

TEST(radio_tuned_rule)
{
    CHECK_INT(DX_RADIO_TUNING_MS, 100);
    CHECK_INT(DX_RADIO_TUNING_MAX_MS, 300);
    /* The status idle at 100 ms: the station starts. */
    CHECK_INT(dx_radio_tuned(1000, 1100, 1), 1);
    CHECK_INT(dx_radio_tuned(1000, 1101, 1), 1);
    /* 99 ms with the status idle: not yet. */
    CHECK_INT(dx_radio_tuned(1000, 1099, 1), 0);
    CHECK_INT(dx_radio_tuned(1000, 1000, 1), 0);
    /* The status busy until 110 ms. */
    CHECK_INT(dx_radio_tuned(1000, 1100, 0), 0);
    CHECK_INT(dx_radio_tuned(1000, 1109, 0), 0);
    CHECK_INT(dx_radio_tuned(1000, 1110, 1), 1);
    /* The status never idle: the limit at 300 ms ends the wait. */
    CHECK_INT(dx_radio_tuned(1000, 1299, 0), 0);
    CHECK_INT(dx_radio_tuned(1000, 1300, 0), 1);
    /* Across the wrap of ticks_ms at 2^31. */
    CHECK_INT(dx_radio_tuned(0x7fffffd0u, 0x33u, 1), 0);
    CHECK_INT(dx_radio_tuned(0x7fffffd0u, 0x34u, 1), 1);
    CHECK_INT(dx_radio_tuned(0x7fffffd0u, 0x34u, 0), 0);
    CHECK_INT(dx_radio_tuned(0x7fffffd0u, 0xfcu, 0), 1);
}
