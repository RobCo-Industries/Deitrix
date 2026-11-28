/* The kind of each sound Deitrix starts: a tune is radio, a recorded
 * station is recordings, every other sound is sfx. */
#include "rc_test.h"

#include <string.h>

#include "fake_platform.h"
#include "services/services.h"
#include "services/sfx_table.h"
#include "ui/ui.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static const int tables[] = {
    DX_SFX_ID_SCROLL,    DX_SFX_ID_SELECT, DX_SFX_ID_MODE,       DX_SFX_ID_TAB,
    DX_SFX_ID_HIGHLIGHT, DX_SFX_ID_TUNER,  DX_SFX_ID_TAPE_START, DX_SFX_ID_TAPE_STOP,
};
#define TABLES ((int)(sizeof(tables) / sizeof(tables[0])))

/* Station 1 is a tune, station 2 a recorded track that loops. */
static dx_radio_t radio;
static unsigned svc_flags;

static void setup(int level)
{
    static const char ini[] = "version = 1\n"
                              "[station.1]\nname = Tune\nkind = tune\nfile = T.TUN\n"
                              "[station.2]\nname = Track\nkind = pcm\nfile = P.PCM\n";
    static const char tune[] = "version 1\nC4 D4 R/8\n";

    dx_radio_stop();
    fake_reset();
    fake.ticks = 1000;
    fake.sysinfo.abi_version = DX_ABI_1_6;
    fake.sysinfo.hw_flags = DX_HW_AUDIO | DX_HW_AUDIO_MID;
    dx_ui_sound_block(0);
    dx_ui_sound_enable(level);
    dx_radio_load(&radio, (const uint8_t *)ini, (int32_t)strlen(ini));
    dx_radio_set_tune(&radio, 0, (const uint8_t *)tune, (int32_t)strlen(tune));
    fake_add_fill("/PIPBOY/DATA/RADIO/P.PCM", 40000, 100);
    dx_radio_set_pcm(&radio, 1, 40000);
    dx_radio_attach(&radio);
    dx_service_radio.init(fake.ticks);
    dx_service_hum.init(fake.ticks);
}

static void radio_tick(uint32_t t)
{
    fake.ticks = t;
    dx_service_radio.tick(t, &svc_flags);
}

static void hum_tick(uint32_t t)
{
    fake.ticks = t;
    dx_service_hum.tick(t, &svc_flags);
}

/* Every entry from `from` on that starts or joins a play has `kind`,
 * and there is at least one. Other entries carry no kind. */
static int plays_are(int from, uint32_t kind)
{
    int n = 0;
    int i;

    for (i = from; i < fake.audio_count; i++) {
        const fake_audio_t *a = &fake.audio[i];
        int plays = a->op == FAKE_AU_SEQ || a->op == FAKE_AU_SAMPLE || a->op == FAKE_AU_QUEUE ||
                    a->op == FAKE_AU_TONE;

        if (plays) {
            if (a->kind != kind) {
                return 0;
            }
            n++;
        } else if (a->kind <= DX_SOUND_GAMES) {
            return 0;
        }
    }
    return n > 0;
}

TEST(tune_starts_as_radio_and_again_when_it_ends)
{
    setup(DX_UI_SOUND_ON);
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    CHECK_INT(fake.seq_count, 1);
    CHECK(plays_are(0, DX_SOUND_RADIO));
    /* The tune ends by itself: the service starts it again. */
    radio_tick(fake.ticks + 10000);
    CHECK_INT(fake.seq_count, 2);
    CHECK(plays_are(0, DX_SOUND_RADIO));
}

TEST(pcm_chunks_and_the_restart_after_an_underrun_are_recordings)
{
    int i;
    int queued = 0;

    setup(DX_UI_SOUND_ON);
    CHECK_INT(dx_radio_play(1, fake.ticks), 0);
    radio_tick(fake.ticks);
    radio_tick(fake.ticks + 500);
    for (i = 0; i < fake.audio_count; i++) {
        queued += fake.audio[i].op == FAKE_AU_QUEUE;
    }
    CHECK(queued >= 1);
    CHECK_INT(fake.audio[0].op, FAKE_AU_SAMPLE);
    CHECK(plays_are(0, DX_SOUND_RECORDINGS));
    /* The queue runs dry: the next chunk starts the sound again. */
    i = fake.audio_count;
    radio_tick(fake.ticks + 30000);
    CHECK_INT(dx_radio_underruns(), 1);
    CHECK_INT(fake.audio[i].op, FAKE_AU_QUEUE);
    CHECK_INT(fake.au_starts, 2);
    CHECK(plays_are(i, DX_SOUND_RECORDINGS));
}

TEST(after_a_stop_of_each_station_ui_sounds_are_sfx)
{
    int st;

    for (st = 0; st < 2; st++) {
        int from;

        setup(DX_UI_SOUND_FULL);
        CHECK_INT(dx_radio_play(st, fake.ticks), 0);
        radio_tick(fake.ticks);
        dx_radio_stop();
        from = fake.audio_count;
        dx_ui_sound_event(DX_SND_MOVE, DX_UI_ROW_MOVED, 0);
        CHECK(plays_are(from, DX_SOUND_SFX));
        dx_ui_sound_enable(DX_UI_SOUND_ON);
        from = fake.audio_count;
        dx_ui_sound(DX_SND_MOVE);
        CHECK_INT(fake.audio[from].op, FAKE_AU_TONE);
        CHECK(plays_are(from, DX_SOUND_SFX));
        dx_ui_sound_enable(DX_UI_SOUND_FULL);
        fake.ticks += 1000;
        from = fake.audio_count;
        CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 1);
        CHECK(plays_are(from, DX_SOUND_SFX));
    }
}

TEST(tuner_is_sfx_then_the_station_has_its_kind)
{
    static const uint32_t kinds[] = { DX_SOUND_RADIO, DX_SOUND_RECORDINGS };
    int st;

    for (st = 0; st < 2; st++) {
        int from;

        setup(DX_UI_SOUND_FULL);
        CHECK_INT(dx_ui_sound_play(DX_SFX_ID_TUNER), 1);
        CHECK(plays_are(0, DX_SOUND_SFX));
        CHECK_INT(dx_radio_tune(st, fake.ticks), 0);
        from = fake.audio_count;
        radio_tick(fake.ticks + 2000);
        CHECK_INT(dx_radio_current(), st);
        CHECK(plays_are(from, kinds[st]));
    }
}

TEST(weep_tables_hum_and_heads_are_sfx)
{
    int i;

    setup(DX_UI_SOUND_ON);
    CHECK_INT(dx_sfx_weep(), 1);
    CHECK(plays_are(0, DX_SOUND_SFX));

    /* Each table, played plain. */
    for (i = 0; i < TABLES; i++) {
        setup(DX_UI_SOUND_FULL);
        CHECK_INT(dx_sfx_play(tables[i]), 1);
        CHECK_INT(fake.audio[1].op, FAKE_AU_SAMPLE);
        CHECK(plays_are(0, DX_SOUND_SFX));
    }

    /* The hum: part one starts, part two joins it. */
    setup(DX_UI_SOUND_FULL_HUM);
    hum_tick(1000 + DX_HUM_POLL_MS);
    hum_tick(1500);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_QUEUE), 1);
    CHECK(plays_are(0, DX_SOUND_SFX));
    /* A head over the hum, and the hum's rest behind it. */
    for (i = 0; i < TABLES; i++) {
        setup(DX_UI_SOUND_FULL_HUM);
        hum_tick(1250);
        fake.audio_count = 0;
        CHECK_INT(dx_sfx_play(tables[i]), 1);
        CHECK_INT(fake.audio[1].op, FAKE_AU_SAMPLE);
        CHECK_INT(fake.audio[2].op, FAKE_AU_QUEUE);
        CHECK(plays_are(0, DX_SOUND_SFX));
    }
}

TEST(hum_next_part_that_restarts_the_play_is_sfx)
{
    int from;

    setup(DX_UI_SOUND_FULL_HUM);
    hum_tick(1250);
    hum_tick(1500);
    /* Long after both parts: the hum starts again. */
    from = fake.audio_count;
    hum_tick(9000);
    CHECK(plays_are(from, DX_SOUND_SFX));
}

TEST(tape_sounds_around_a_launch_are_sfx)
{
    setup(DX_UI_SOUND_FULL);
    CHECK_INT(dx_sfx_play(DX_SFX_ID_TAPE_START), 1);
    CHECK_INT(dx_plat_holo_launch("CULVERT.HTP", ""), 0);
    fake.ticks += 5000;
    CHECK_INT(dx_sfx_play(DX_SFX_ID_TAPE_STOP), 1);
    CHECK(plays_are(0, DX_SOUND_SFX));
}

TEST(fake_refuses_an_unknown_kind)
{
    static const uint8_t b[] = { 128 };
    static const dx_note_t n[] = { { 440, 10 } };

    setup(DX_UI_SOUND_ON);
    CHECK_INT(dx_plat_sample(4, b, 1), DX_EINVAL);
    CHECK_INT(dx_plat_sample_queue(4, b, 1), DX_EINVAL);
    CHECK_INT(dx_plat_play_seq(4, n, 1), DX_EINVAL);
    dx_plat_tone(4, 440, 10);
    CHECK_INT(fake.audio_count, 0);
    /* A queued sample that joins a play keeps the kind of that play. */
    CHECK_INT(dx_plat_sample(DX_SOUND_GAMES, b, 1), 0);
    CHECK_INT(dx_plat_sample_queue(DX_SOUND_RADIO, b, 1), 0);
    CHECK_INT(fake.audio[1].op, FAKE_AU_QUEUE);
    CHECK_INT(fake.audio[1].kind, DX_SOUND_GAMES);
    dx_plat_audio_stop();
    CHECK_INT(fake.audio[2].kind, FAKE_KIND_NONE);
}
