/* The hum: the heads against the generated header, the top-up as a pure
 * state machine, and the service on the fake audio. */
#include "rc_test.h"

#include <string.h>

#include <robco/crc32.h>

#include "fake_platform.h"
#include "services/services.h"
#include "services/sfx_table.h"
#include "ui/ui.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

/* Each one-shot's id and the header's lines for its head and rest. */
static const struct {
    int id;
    uint32_t head_len, head_crc, rest_len, rest_crc;
} shots[] = {
    { DX_SFX_ID_SCROLL, DX_SFX_SCROLL_HEAD_LEN, DX_SFX_SCROLL_HEAD_CRC32,
      DX_SFX_SCROLL_REST_LEN, DX_SFX_SCROLL_REST_CRC32 },
    { DX_SFX_ID_SELECT, DX_SFX_SELECT_HEAD_LEN, DX_SFX_SELECT_HEAD_CRC32,
      DX_SFX_SELECT_REST_LEN, DX_SFX_SELECT_REST_CRC32 },
    { DX_SFX_ID_MODE, DX_SFX_MODE_HEAD_LEN, DX_SFX_MODE_HEAD_CRC32, DX_SFX_MODE_REST_LEN,
      DX_SFX_MODE_REST_CRC32 },
    { DX_SFX_ID_TAB, DX_SFX_TAB_HEAD_LEN, DX_SFX_TAB_HEAD_CRC32, DX_SFX_TAB_REST_LEN,
      DX_SFX_TAB_REST_CRC32 },
    { DX_SFX_ID_HIGHLIGHT, DX_SFX_HIGHLIGHT_HEAD_LEN, DX_SFX_HIGHLIGHT_HEAD_CRC32,
      DX_SFX_HIGHLIGHT_REST_LEN, DX_SFX_HIGHLIGHT_REST_CRC32 },
    { DX_SFX_ID_TUNER, DX_SFX_TUNER_HEAD_LEN, DX_SFX_TUNER_HEAD_CRC32, DX_SFX_TUNER_REST_LEN,
      DX_SFX_TUNER_REST_CRC32 },
    { DX_SFX_ID_TAPE_START, DX_SFX_TAPE_START_HEAD_LEN, DX_SFX_TAPE_START_HEAD_CRC32,
      DX_SFX_TAPE_START_REST_LEN, DX_SFX_TAPE_START_REST_CRC32 },
    { DX_SFX_ID_TAPE_STOP, DX_SFX_TAPE_STOP_HEAD_LEN, DX_SFX_TAPE_STOP_HEAD_CRC32,
      DX_SFX_TAPE_STOP_REST_LEN, DX_SFX_TAPE_STOP_REST_CRC32 },
};
#define SHOTS ((int)(sizeof(shots) / sizeof(shots[0])))

static dx_radio_t radio;

/* A kernel where tables play, "Full + hum", one tune station, the hum
 * service started at tick 1000. */
static void setup(int level)
{
    static const char ini[] = "version = 1\n[station.1]\nname = Tune\nkind = tune\nfile = T.TUN\n";
    static const char tune[] = "version 1\nC4 D4 R/8\n";

    fake_reset();
    fake.ticks = 1000;
    fake.sysinfo.abi_version = DX_ABI_1_5;
    fake.sysinfo.hw_flags = DX_HW_AUDIO | DX_HW_AUDIO_MID;
    dx_ui_sound_block(0);
    dx_ui_sound_enable(level);
    dx_radio_load(&radio, (const uint8_t *)ini, (int32_t)strlen(ini));
    dx_radio_set_tune(&radio, 0, (const uint8_t *)tune, (int32_t)strlen(tune));
    dx_radio_attach(&radio);
    dx_service_radio.init(fake.ticks);
    dx_service_hum.init(fake.ticks);
}

static uint32_t tick_at(uint32_t t)
{
    unsigned flags = 0;

    fake.ticks = t;
    return dx_service_hum.tick(t, &flags);
}

/* The last sample or queue call. */
static const fake_audio_t *last_play(void)
{
    int i;

    for (i = fake.audio_count - 1; i >= 0; i--) {
        if (fake.audio[i].op == FAKE_AU_SAMPLE || fake.audio[i].op == FAKE_AU_QUEUE) {
            return &fake.audio[i];
        }
    }
    return NULL;
}

/* ---- The heads ------------------------------------------------------------ */

TEST(heads_match_the_header)
{
    int i;

    CHECK_STR(DX_SFX_BED_NAME, "hum");
    for (i = 0; i < SHOTS; i++) {
        uint32_t n = 0;
        const uint8_t *h = dx_sfx_head(shots[i].id, &n);

        CHECK(h != NULL);
        CHECK_INT(n, shots[i].head_len);
        CHECK(n <= DX_SFX_BED_HEAD_MAX);
        CHECK_INT(crc32(h, n), shots[i].head_crc);
        /* The rest is the hum's first part after the head. */
        CHECK_INT(DX_SFX_HUM_1_LEN - n, shots[i].rest_len);
        CHECK_INT(crc32(dx_sfx_hum_1 + n, DX_SFX_HUM_1_LEN - n), shots[i].rest_crc);
    }
    CHECK(dx_sfx_head(-1, NULL) == NULL);
    CHECK(dx_sfx_head(DX_SFX_COUNT, NULL) == NULL);
}

TEST(mix_rule_and_clamp)
{
    static const uint8_t shot[] = { 130, 136, 126, 250, 5, 128 };
    static const uint8_t bed[] = { 128, 131, 128, 140, 100, 0 };
    uint8_t out[6];

    dx_sfx_mix(out, shot, bed, 6);
    CHECK_INT(out[0], 130);
    CHECK_INT(out[1], 139);
    CHECK_INT(out[2], 126);
    CHECK_INT(out[3], 255); /* 262 clamps */
    CHECK_INT(out[4], 0);   /* -23 clamps */
    CHECK_INT(out[5], 0);
}

TEST(heads_fit_their_buffer)
{
    uint32_t sum = 0;
    int i;

    for (i = 0; i < SHOTS; i++) {
        sum += shots[i].head_len;
    }
    CHECK_INT(DX_SFX_HEADS_SIZE, sum);
}

/* The rest behind the longest head outlasts a top-up period plus a
 * full panel frame, and 500 ms. */
TEST(rest_behind_the_longest_head_lasts)
{
    uint32_t shortest = DX_SFX_HUM_1_LEN;
    int i;

    for (i = 0; i < SHOTS; i++) {
        shortest = shots[i].rest_len < shortest ? shots[i].rest_len : shortest;
    }
    CHECK_INT(shortest, DX_SFX_HUM_1_LEN - DX_SFX_BED_HEAD_MAX);
    CHECK(shortest * 1000u / DX_SFX_RATE_HZ >= 500u);
    CHECK(shortest * 1000u / DX_SFX_RATE_HZ > DX_HUM_POLL_MS + DX_HUM_FRAME_MS);
    /* Each part of the hum outlasts a period too. */
    CHECK(DX_SFX_HUM_2_LEN * 1000u / DX_SFX_RATE_HZ > DX_HUM_POLL_MS + DX_HUM_FRAME_MS);
}

TEST(loudest_one_shot_over_the_hum_fits)
{
    static const unsigned peaks[] = {
        DX_SFX_SCROLL_PEAK, DX_SFX_SELECT_PEAK, DX_SFX_MODE_PEAK, DX_SFX_TAB_PEAK,
        DX_SFX_HIGHLIGHT_PEAK, DX_SFX_TUNER_PEAK, DX_SFX_TAPE_START_PEAK, DX_SFX_TAPE_STOP_PEAK,
    };
    unsigned top = 0;
    size_t i;

    for (i = 0; i < sizeof(peaks) / sizeof(peaks[0]); i++) {
        top = peaks[i] > top ? peaks[i] : top;
    }
    CHECK(top + DX_SFX_HUM_PEAK <= 127);
}

/* ---- The top-up as a state machine ---------------------------------------- */

TEST(step_states)
{
    dx_hum_t h = { 0, 0 };

    /* Off: nothing, and nothing to stop. */
    CHECK_INT(dx_hum_step(&h, 0, 0, 0), DX_HUM_NONE);
    /* On, silence: part one starts, part two comes next. */
    CHECK_INT(dx_hum_step(&h, 1, 0, 0), DX_HUM_START);
    CHECK_INT(h.running, 1);
    CHECK_INT(h.next, 1);
    /* Busy: queue the next part. */
    CHECK_INT(dx_hum_step(&h, 1, 0, 1), DX_HUM_QUEUE);
    /* Ran dry: start again from part one. */
    h.next = 0;
    CHECK_INT(dx_hum_step(&h, 1, 0, 0), DX_HUM_START);
    CHECK_INT(h.next, 1);
    /* The radio: the hum stops running, the radio's play took the voice. */
    CHECK_INT(dx_hum_step(&h, 1, 1, 1), DX_HUM_NONE);
    CHECK_INT(h.running, 0);
    CHECK_INT(dx_hum_step(&h, 1, 1, 0), DX_HUM_NONE);
    /* The radio stops while its last sound plays: wait for the end. */
    CHECK_INT(dx_hum_step(&h, 1, 0, 1), DX_HUM_NONE);
    CHECK_INT(dx_hum_step(&h, 1, 0, 0), DX_HUM_START);
    /* The setting leaves "Full + hum": stop once. */
    CHECK_INT(dx_hum_step(&h, 0, 0, 1), DX_HUM_STOP);
    CHECK_INT(dx_hum_step(&h, 0, 0, 1), DX_HUM_NONE);
    /* Another sound took the voice: wait for it, then start. */
    h.running = 0;
    CHECK_INT(dx_hum_step(&h, 1, 0, 1), DX_HUM_NONE);
    CHECK_INT(dx_hum_step(&h, 1, 0, 0), DX_HUM_START);
}

TEST(leave_states)
{
    dx_hum_t h = { 1, 0 };

    /* The hum plays, either part next: one stop, then nothing. */
    CHECK_INT(dx_hum_leave(&h, 1), DX_HUM_STOP);
    CHECK_INT(h.running, 0);
    CHECK_INT(dx_hum_leave(&h, 1), DX_HUM_NONE);
    h.running = 1;
    h.next = 1;
    CHECK_INT(dx_hum_leave(&h, 1), DX_HUM_STOP);
    CHECK_INT(h.running, 0);
    CHECK_INT(h.next, 0);
    /* Another sound holds the voice: the hum was halted, so no stop. */
    CHECK_INT(dx_hum_leave(&h, 1), DX_HUM_NONE);
    /* The hum ran dry: nothing to stop. */
    h.running = 1;
    CHECK_INT(dx_hum_leave(&h, 0), DX_HUM_NONE);
    CHECK_INT(h.running, 0);
    /* The setting off afterwards asks nothing more. */
    CHECK_INT(dx_hum_step(&h, 0, 0, 1), DX_HUM_NONE);
}

/* ---- The service on the fake audio ---------------------------------------- */

TEST(service_starts_after_a_period_and_alternates)
{
    const fake_audio_t *a;

    setup(DX_UI_SOUND_FULL_HUM);
    /* A key wakes every service: before its period the hum does nothing. */
    CHECK_INT(tick_at(1000), 1000 + DX_HUM_POLL_MS);
    CHECK_INT(fake.audio_count, 0);
    CHECK_INT(tick_at(1000 + DX_HUM_POLL_MS), 1000 + 2 * DX_HUM_POLL_MS);
    a = last_play();
    CHECK(a != NULL);
    CHECK_INT(a->op, FAKE_AU_SAMPLE);
    CHECK_INT(a->n, DX_SFX_HUM_1_LEN);
    CHECK_INT(a->rate, DX_RATE_HI);
    CHECK_INT(fake.rate, DX_RATE_LO);
    CHECK_INT(dx_hum_running(), 1);
    /* The next top-up queues part two, the one after it is refused
     * while part two waits, then part one follows. */
    tick_at(1500);
    a = last_play();
    CHECK_INT(a->op, FAKE_AU_QUEUE);
    CHECK_INT(a->n, DX_SFX_HUM_2_LEN);
    CHECK_INT(a->rate, DX_RATE_HI);
    CHECK_INT(fake.au_nexts, 1);
    tick_at(1750);
    CHECK_INT(fake.audio[fake.audio_count - 2].op, FAKE_AU_BUSY);
    CHECK_INT(fake.au_nexts, 1);
    /* Part one ends at 1250 + 1155: part two takes over, part one queues. */
    tick_at(2500);
    a = last_play();
    CHECK_INT(a->op, FAKE_AU_QUEUE);
    CHECK_INT(a->n, DX_SFX_HUM_1_LEN);
    CHECK_INT(fake.au_nexts, 2);
    CHECK_INT(fake.au_starts, 1);
    CHECK_INT(fake.rate, DX_RATE_LO);
}

TEST(service_restarts_when_dry)
{
    setup(DX_UI_SOUND_FULL_HUM);
    tick_at(1250);
    /* Far past both parts: the next top-up starts part one again. */
    tick_at(9000);
    CHECK_INT(last_play()->op, FAKE_AU_SAMPLE);
    CHECK_INT(last_play()->n, DX_SFX_HUM_1_LEN);
    CHECK_INT(fake.au_starts, 2);
}

TEST(table_over_the_hum_is_head_and_rest)
{
    int i;

    for (i = 0; i < SHOTS; i++) {
        uint32_t crc;

        setup(DX_UI_SOUND_FULL_HUM);
        tick_at(1250);
        fake.audio_count = 0;
        fake.au_crc = 0;
        CHECK_INT(dx_sfx_play(shots[i].id), 1);
        CHECK_INT(fake.audio_count, 4);
        CHECK_INT(fake.audio[0].op, FAKE_AU_RATE);
        CHECK_INT(fake.audio[1].op, FAKE_AU_SAMPLE);
        CHECK_INT(fake.audio[1].n, shots[i].head_len);
        CHECK_INT(fake.audio[1].rate, DX_RATE_HI);
        CHECK_INT(fake.audio[2].op, FAKE_AU_QUEUE);
        CHECK_INT(fake.audio[2].n, shots[i].rest_len);
        CHECK_INT(fake.audio[2].rate, DX_RATE_HI);
        CHECK_INT(fake.audio[3].op, FAKE_AU_RATE);
        CHECK_INT(fake.rate, DX_RATE_LO);
        /* The bytes the kernel took: the head, then the rest. */
        crc = crc32_update(0, dx_sfx_head(shots[i].id, &(uint32_t){ 0 }), shots[i].head_len);
        crc = crc32_update(crc, dx_sfx_hum_1 + shots[i].head_len, shots[i].rest_len);
        CHECK_INT(fake.au_crc, crc);
        CHECK_INT(dx_hum_running(), 1);
        CHECK_INT(dx_hum_state()->next, 1);
    }
}

TEST(full_without_hum_plays_the_plain_table)
{
    setup(DX_UI_SOUND_FULL);
    tick_at(1250);
    tick_at(1500);
    CHECK_INT(fake.audio_count, 0);
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 1);
    CHECK_INT(last_play()->op, FAKE_AU_SAMPLE);
    CHECK_INT(last_play()->n, DX_SFX_SCROLL_LEN);
    CHECK_INT(fake_audio_ops(FAKE_AU_QUEUE), 0);
    /* "Full + hum" before the hum starts: the plain table too. */
    setup(DX_UI_SOUND_FULL_HUM);
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 1);
    CHECK_INT(fake.au_crc, DX_SFX_SCROLL_CRC32);
    CHECK_INT(fake_audio_ops(FAKE_AU_QUEUE), 0);
}

TEST(value_3_without_the_flag_plays_tones_and_no_hum)
{
    setup(DX_UI_SOUND_FULL_HUM);
    fake.sysinfo.hw_flags = DX_HW_AUDIO;
    tick_at(1250);
    tick_at(1500);
    CHECK_INT(fake.audio_count, 0);
    CHECK_INT(dx_hum_running(), 0);
    dx_ui_sound_event(DX_SND_MOVE, DX_UI_ROW_MOVED, 0);
    CHECK_INT(fake.tone_count, 1);
    CHECK_INT(fake.tones[0][0], 1400);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
    CHECK_STR(dx_ui_sound_label(DX_UI_SOUND_FULL_HUM, 0), "On");
}

TEST(a_tone_waits_out_then_the_hum_comes_back)
{
    setup(DX_UI_SOUND_FULL_HUM);
    tick_at(1250);
    fake.ticks = 1300;
    dx_ui_sound_event(DX_SND_DENY, DX_UI_IGNORED, 0);
    CHECK_INT(fake.tone_count, 1);
    CHECK_INT(dx_hum_running(), 0);
    /* The tone plays 70 ms: no top-up cuts it, a later one restarts. */
    fake.audio_count = 0;
    tick_at(1310);
    CHECK_INT(fake.audio_count, 0);
    tick_at(1500);
    CHECK_INT(last_play()->op, FAKE_AU_SAMPLE);
    CHECK_INT(last_play()->n, DX_SFX_HUM_1_LEN);
}

TEST(radio_stops_the_hum_and_its_end_brings_it_back)
{
    unsigned flags = 0;

    setup(DX_UI_SOUND_FULL_HUM);
    tick_at(1250);
    /* A station: the hum stops queueing. The tune replaced it. */
    fake.ticks = 1300;
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    CHECK_INT(fake.seq_count, 1);
    fake.audio_count = 0;
    tick_at(1500);
    tick_at(1750);
    CHECK_INT(dx_hum_running(), 0);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE) + fake_audio_ops(FAKE_AU_QUEUE), 0);
    CHECK_INT(fake_audio_ops(FAKE_AU_RATE), 0);
    /* The tune replays: the radio's own sound. */
    dx_service_radio.tick(fake.ticks + 5000, &flags);
    /* The station stops: the next top-up starts the hum again. */
    dx_radio_stop();
    tick_at(8000);
    CHECK_INT(dx_hum_running(), 1);
    CHECK_INT(last_play()->op, FAKE_AU_SAMPLE);
    CHECK_INT(last_play()->rate, DX_RATE_HI);
    CHECK_INT(fake.rate, DX_RATE_LO);
}

TEST(setting_off_stops_the_hum)
{
    setup(DX_UI_SOUND_FULL_HUM);
    tick_at(1250);
    dx_ui_sound_enable(DX_UI_SOUND_OFF);
    tick_at(1500);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
    CHECK_INT(dx_hum_running(), 0);
    tick_at(1750);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
}

TEST(a_tape_stops_the_hum_until_its_stop_sound_ends)
{
    setup(DX_UI_SOUND_FULL_HUM);
    tick_at(1250);
    /* As the launch does: halt, the start sound plays plain. */
    fake.ticks = 1300;
    dx_hum_halt();
    CHECK_INT(dx_sfx_play(DX_SFX_ID_TAPE_START), 1);
    CHECK_INT(last_play()->n, DX_SFX_TAPE_START_LEN);
    CHECK_INT(fake_audio_ops(FAKE_AU_QUEUE), 0);
    /* Pip-OS stops the sound at the launch and at the return. */
    dx_plat_audio_stop();
    fake.ticks = 20000;
    CHECK_INT(dx_sfx_play(DX_SFX_ID_TAPE_STOP), 1);
    CHECK_INT(last_play()->n, DX_SFX_TAPE_STOP_LEN);
    /* The stop sound lasts 415 ms: the hum waits for it. */
    fake.audio_count = 0;
    tick_at(20100);
    CHECK_INT(fake.audio_count, 0);
    tick_at(20500);
    CHECK_INT(last_play()->op, FAKE_AU_SAMPLE);
    CHECK_INT(last_play()->n, DX_SFX_HUM_1_LEN);
}

/* ---- The rate in both orders ---------------------------------------------- */

static dx_radio_t pcm_radio;

static void add_pcm_station(void)
{
    static const char ini[] = "version = 1\n[station.1]\nname = Track\nkind = pcm\nfile = P.PCM\n";

    dx_radio_load(&pcm_radio, (const uint8_t *)ini, (int32_t)strlen(ini));
    fake_add_fill("/PIPBOY/DATA/RADIO/P.PCM", 24000, 100);
    dx_radio_set_pcm(&pcm_radio, 0, 24000);
    dx_radio_attach(&pcm_radio);
}

TEST(rates_hum_then_radio_then_hum)
{
    int i;
    int hum_plays = 0, radio_plays = 0;

    setup(DX_UI_SOUND_FULL_HUM);
    add_pcm_station();
    tick_at(1250);
    tick_at(1500);
    /* The track streams at 8000 Hz. */
    fake.ticks = 1600;
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    tick_at(1750);
    dx_radio_stop();
    tick_at(2000);
    tick_at(2250);
    CHECK_INT(dx_hum_running(), 1);
    for (i = 0; i < fake.audio_count; i++) {
        const fake_audio_t *a = &fake.audio[i];

        if (a->op != FAKE_AU_SAMPLE && a->op != FAKE_AU_QUEUE) {
            continue;
        }
        if (a->first == 100) {
            CHECK_INT(a->rate, DX_RATE_LO);
            radio_plays++;
        } else {
            CHECK_INT(a->rate, DX_RATE_HI);
            hum_plays++;
        }
    }
    CHECK(radio_plays >= 1);
    CHECK(hum_plays >= 3);
    CHECK_INT(fake.rate, DX_RATE_LO);
}

TEST(rates_radio_first_then_hum)
{
    int i, seen_hum = 0;

    setup(DX_UI_SOUND_FULL_HUM);
    add_pcm_station();
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    tick_at(1250);
    CHECK_INT(dx_hum_running(), 0);
    dx_radio_stop();
    tick_at(1500);
    tick_at(1750);
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    for (i = 0; i < fake.audio_count; i++) {
        const fake_audio_t *a = &fake.audio[i];

        if (a->op == FAKE_AU_SAMPLE || a->op == FAKE_AU_QUEUE) {
            CHECK_INT(a->rate, a->first == 100 ? DX_RATE_LO : DX_RATE_HI);
            seen_hum += a->first != 100;
        }
    }
    CHECK(seen_hum >= 2);
    CHECK_INT(fake.rate, DX_RATE_LO);
    dx_radio_stop();
    dx_ui_sound_enable(DX_UI_SOUND_ON);
}


/* Plays after the leave: samples and queued buffers. */
static int plays_since(int from)
{
    int i, n = 0;

    for (i = from; i < fake.audio_count; i++) {
        n += fake.audio[i].op == FAKE_AU_SAMPLE || fake.audio[i].op == FAKE_AU_QUEUE;
    }
    return n;
}

TEST(leaving_full_hum_stops_at_once)
{
    static const int levels[] = { DX_UI_SOUND_OFF, DX_UI_SOUND_ON, DX_UI_SOUND_FULL };
    int i, at;

    for (i = 0; i < 3; i++) {
        /* Part one plays, then part two queued too. */
        int queued;

        for (queued = 0; queued < 2; queued++) {
            setup(DX_UI_SOUND_FULL_HUM);
            tick_at(1250);
            if (queued) {
                tick_at(1500);
            }
            CHECK_INT(dx_hum_running(), 1);
            at = fake.audio_count;
            dx_ui_sound_enable(levels[i]);
            /* The same pass: no top-up ran. */
            CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
            CHECK_INT(dx_hum_running(), 0);
            CHECK(strstr(fake.log, "deitrix: sound left hum t_ms=1") != NULL);
            tick_at(1750);
            tick_at(2000);
            CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
            CHECK_INT(plays_since(at), 0);
        }
    }
}

/* A head is part hum and its rest is the hum: the stop cuts both. */
TEST(leaving_cuts_a_head_and_no_hum_part_follows)
{
    int at;

    setup(DX_UI_SOUND_FULL_HUM);
    tick_at(1250);
    CHECK_INT(dx_sfx_play(DX_SFX_ID_SCROLL), 1);
    at = fake.audio_count;
    dx_ui_sound_enable(DX_UI_SOUND_OFF);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
    CHECK_INT(fake.audio[at].op, FAKE_AU_STOP);
    tick_at(1500);
    tick_at(1750);
    CHECK_INT(plays_since(at), 0);
}

/* A tone halted the hum: leaving cuts nothing. */
TEST(leaving_spares_a_tone)
{
    setup(DX_UI_SOUND_FULL_HUM);
    tick_at(1250);
    dx_ui_sound(DX_SND_MOVE);
    dx_ui_sound_enable(DX_UI_SOUND_ON);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 0);
    CHECK(strstr(fake.log, "deitrix: sound left hum") != NULL);
}

/* Entering waits for the next top-up. */
TEST(entering_full_hum_waits_for_the_top_up)
{
    setup(DX_UI_SOUND_FULL);
    tick_at(1250);
    dx_ui_sound_enable(DX_UI_SOUND_FULL_HUM);
    CHECK_INT(fake.au_starts, 0);
    tick_at(1500);
    CHECK_INT(fake.au_starts, 1);
    CHECK_INT(dx_hum_running(), 1);
}
