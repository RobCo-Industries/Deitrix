/* The radio and autosave services against the fake audio, card and clock. */
#include "rc_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <robco/crc32.h>

#include "fake_platform.h"
#include "services/services.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static dx_radio_t radio;

/* Stations: 1 a tune, 2 a looping track, 3 a track played once, 4 a
 * track whose file is missing. */
static void setup(uint32_t track_size, int loop2)
{
    static const char ini[] =
        "version = 1\n"
        "[station.1]\nname = Tune\nkind = tune\nfile = T.TUN\n"
        "[station.2]\nname = Loop\nkind = pcm\nfile = L.PCM\n"
        "[station.3]\nname = Once\nkind = pcm\nfile = O.PCM\nloop = no\n"
        "[station.4]\nname = Gone\nkind = pcm\nfile = G.PCM\n";
    static const char tune[] = "version 1\nC4 D4 R/8\n";
    static uint8_t *track;
    uint32_t i;

    fake_reset();
    dx_ui_sound_block(0);
    dx_radio_load(&radio, (const uint8_t *)ini, (int32_t)strlen(ini));
    dx_radio_set_tune(&radio, 0, (const uint8_t *)tune, (int32_t)strlen(tune));
    free(track);
    track = malloc(track_size);
    for (i = 0; i < track_size; i++) {
        track[i] = (uint8_t)(i * 7 + i / 300);
    }
    fake_add_file("/PIPBOY/DATA/RADIO/L.PCM", track, track_size);
    fake_add_file("/PIPBOY/DATA/RADIO/O.PCM", track, track_size);
    dx_radio_set_pcm(&radio, 1, (int32_t)track_size);
    dx_radio_set_pcm(&radio, 2, (int32_t)track_size);
    dx_radio_set_pcm(&radio, 3, DX_FILE_MISSING);
    if (!loop2) {
        radio.stations[1].loop = 0;
    }
    dx_radio_attach(&radio);
    dx_service_radio.init(fake.ticks);
}

/* Runs the service as the loop would: tick at each wake time, then
 * moves the clock to the next one. */
static void run_for(uint32_t ms, uint32_t wake)
{
    uint32_t end = fake.ticks + ms;

    while (wake != 0 && fake.ticks < end) {
        if (dx_ticks_due(fake.ticks, wake)) {
            wake = dx_service_radio.tick(fake.ticks);
        } else {
            fake.ticks += dx_ticks_since(fake.ticks, wake);
        }
    }
    if (fake.ticks < end) {
        fake.ticks = end;
    }
}

static uint32_t loop_wake;

/* The same, but the wake keeps across calls. */
static void play_for(uint32_t ms)
{
    uint32_t end = (fake.ticks + ms) & DX_TICKS_MASK;

    while (loop_wake != 0 && fake.ticks != end) {
        if (dx_ticks_due(fake.ticks, loop_wake)) {
            loop_wake = dx_service_radio.tick(fake.ticks);
        } else {
            uint32_t step = dx_ticks_since(fake.ticks, loop_wake);
            uint32_t left = dx_ticks_since(fake.ticks, end);

            fake.ticks = (fake.ticks + (step < left ? step : left)) & DX_TICKS_MASK;
        }
    }
    fake.ticks = end;
}

TEST(tune_plays_and_replays)
{
    setup(16000, 1);
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_TUNE);
    CHECK_INT(dx_radio_current(), 0);
    CHECK_INT(fake.seq_count, 1);
    CHECK_INT(fake.seq_ms, 1250);
    CHECK_INT(fake.audio[0].first, 262);
    /* UI tones do not cut the radio. */
    dx_ui_sound(DX_SND_MOVE);
    CHECK_INT(fake.tone_count, 0);
    loop_wake = dx_service_radio.tick(fake.ticks);
    play_for(1240);
    CHECK_INT(fake.seq_count, 1);
    play_for(20);
    CHECK_INT(fake.seq_count, 2); /* it ended: it plays again */
    play_for(1250 * 3);
    CHECK_INT(fake.seq_count, 5);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 0);
}

TEST(tune_waits_while_the_sound_lasts)
{
    setup(16000, 1);
    dx_radio_play(0, fake.ticks);
    /* The kernel plays late: the replay waits for the sound to end. */
    fake.au_end = (fake.au_end + 500) & DX_TICKS_MASK;
    loop_wake = dx_service_radio.tick(fake.ticks);
    play_for(1300);
    CHECK_INT(fake.seq_count, 1);
    play_for(500);
    CHECK_INT(fake.seq_count, 2);
}

/* The bytes that reached the kernel are the file's, pass after pass. */
static uint32_t stream_crc(uint32_t size, uint32_t bytes)
{
    const fake_file_t *f = NULL;
    uint32_t crc = 0;
    int i;

    for (i = 0; i < fake.file_count; i++) {
        if (strcmp(fake.files[i].path, "/PIPBOY/DATA/RADIO/L.PCM") == 0) {
            f = &fake.files[i];
        }
    }
    while (f != NULL && bytes > 0) {
        uint32_t n = bytes < size ? bytes : size;

        crc = crc32_update(crc, f->data, n);
        bytes -= n;
    }
    return crc;
}

TEST(pcm_streams_in_chunks)
{
    setup(40000, 1); /* five chunks */
    CHECK_INT(dx_radio_play(1, fake.ticks), 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_PCM);
    /* One plays, one waits in the kernel, one waits in RAM. */
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_QUEUE), 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_BUSY), 1);
    CHECK_INT(fake.audio[0].n, DX_RADIO_CHUNK);
    loop_wake = dx_service_radio.tick(fake.ticks);
    CHECK_INT(dx_ticks_since(fake.ticks, loop_wake), DX_RADIO_POLL_MS);
    play_for(10000);
    CHECK_INT(fake.au_starts, 1);
    CHECK_INT(dx_radio_underruns(), 0);
    CHECK(fake.au_nexts >= 10);
    CHECK_INT(fake.au_crc, stream_crc(40000, fake.au_bytes));
    CHECK_INT(fake.handles_open, 1);
    dx_radio_stop();
    CHECK_INT(fake.handles_open, 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
}

/* A track that is not a whole number of chunks: the chunk at the end
 * fills on from the start of the next pass, so no chunk is short. */
TEST(pcm_loop_join)
{
    setup(8000 * 2 + 1234, 1);
    dx_radio_play(1, fake.ticks);
    loop_wake = dx_service_radio.tick(fake.ticks);
    play_for(12000);
    CHECK_INT(fake.au_starts, 1);
    CHECK_INT(dx_radio_underruns(), 0);
    CHECK(fake.au_bytes > 3 * (8000 * 2 + 1234));
    CHECK_INT(fake.au_crc, stream_crc(8000 * 2 + 1234, fake.au_bytes));
    CHECK_INT(fake.handles_open, 1);
    for (int i = 0; i < fake.audio_count; i++) {
        CHECK(fake.audio[i].op != FAKE_AU_QUEUE || fake.audio[i].n == DX_RADIO_CHUNK);
    }
    /* A track of exactly one chunk loops too. */
    setup(8000, 1);
    dx_radio_play(1, fake.ticks);
    loop_wake = dx_service_radio.tick(fake.ticks);
    play_for(5000);

    CHECK_INT(fake.au_starts, 1);
    CHECK_INT(fake.au_crc, stream_crc(8000, fake.au_bytes));
}

TEST(pcm_busy_answers_keep_the_chunk)
{
    int busy;

    setup(80000, 1);
    dx_radio_play(1, fake.ticks);
    loop_wake = dx_service_radio.tick(fake.ticks);
    play_for(3000);
    busy = fake_audio_ops(FAKE_AU_BUSY);
    CHECK(busy >= 3);
    /* Each busy chunk is sent again, not skipped: the stream is whole. */
    CHECK_INT(fake.au_crc, stream_crc(80000, fake.au_bytes));
}

TEST(pcm_underrun_restarts)
{
    setup(80000, 1);
    dx_radio_play(1, fake.ticks);
    loop_wake = dx_service_radio.tick(fake.ticks);
    play_for(1500);
    /* The ROM stalls for 3 s: the queue runs dry. */
    fake.ticks += 3000;
    loop_wake = dx_service_radio.tick(fake.ticks);
    CHECK_INT(dx_radio_underruns(), 1);
    CHECK_INT(fake.au_starts, 2); /* the next chunk started the sound again */
    CHECK(strstr(fake.log, "deitrix: radio underrun\n") != NULL);
    play_for(4000);
    CHECK_INT(dx_radio_underruns(), 1);
    CHECK_INT(fake.au_crc, stream_crc(80000, fake.au_bytes));
}

TEST(pcm_once_ends)
{
    setup(20000, 1);
    CHECK_INT(dx_radio_play(2, fake.ticks), 0);
    loop_wake = dx_service_radio.tick(fake.ticks);
    play_for(2400);
    CHECK_INT(dx_radio_state(), DX_RADIO_PCM);
    play_for(1000);
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(loop_wake, 0);
    CHECK_INT(fake.au_bytes, 20000);
    CHECK_INT(fake.handles_open, 0);
    /* UI sounds play again. */
    dx_ui_sound(DX_SND_MOVE);
    CHECK_INT(fake.tone_count, 1);
}

TEST(stop_in_every_state)
{
    setup(40000, 1);
    dx_radio_stop(); /* off: no audio call */
    CHECK_INT(fake.audio_count, 0);
    dx_radio_play(0, fake.ticks);
    dx_radio_stop();
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 1);
    CHECK_INT(fake_audio_busy(), 0);
    dx_radio_play(1, fake.ticks);
    run_for(1200, dx_service_radio.tick(fake.ticks));
    dx_radio_stop();
    CHECK_INT(fake_audio_ops(FAKE_AU_STOP), 2);
    CHECK_INT(fake.handles_open, 0);
    CHECK_INT(dx_service_radio.tick(fake.ticks), 0);
    /* Playing another station stops the first one. */
    dx_radio_play(1, fake.ticks);
    dx_radio_play(0, fake.ticks);
    CHECK_INT(fake.handles_open, 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_TUNE);
    dx_service_radio.stop();
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    dx_ui_sound(DX_SND_MOVE);
    CHECK_INT(fake.tone_count, 1);
}

TEST(station_cannot_play)
{
    setup(40000, 1);
    CHECK_INT(dx_radio_play(3, fake.ticks), -1); /* file missing */
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(dx_radio_play(9, fake.ticks), -1);
    CHECK_INT(dx_radio_play(-1, fake.ticks), -1);
    /* Listed with a size, but gone from the card when played. */
    fake_remove_file("/PIPBOY/DATA/RADIO/L.PCM");
    CHECK_INT(dx_radio_play(1, fake.ticks), -1);
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(fake.audio_count, 0);
    /* A read error while streaming stops the radio. */
    setup(40000, 1);
    dx_radio_play(1, fake.ticks);
    fake.read_fail = 1;
    run_for(3000, dx_service_radio.tick(fake.ticks));
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK_INT(fake.handles_open, 0);
}

TEST(volume)
{
    setup(8000, 1);
    CHECK_INT(dx_radio_set_volume(40), 0);
    CHECK_INT(fake.volume, 40);
    CHECK_INT(dx_radio_set_volume(101), -1);
    CHECK_INT(dx_radio_set_volume(-1), -1);
    CHECK_INT(fake.volume, 40);
    CHECK_INT(dx_radio_set_volume(0), 0);
    CHECK_INT(fake.volume, 0);
}

/* ---- Autosave ------------------------------------------------------------ */

static dx_settings_t settings;
static dx_items_t items;

static void save_setup(void)
{
    static const char ini[] = "version = 1\n[categories]\nm = MISC\n[item.a]\nname = A\ncat = m\n";

    fake_reset();
    dx_settings_defaults(&settings, 50);
    dx_items_load(&items, (const uint8_t *)ini, (int32_t)strlen(ini));
    dx_autosave_attach(&settings, &items);
    dx_service_autosave.init(fake.ticks);
}

TEST(autosave_quiet_period)
{
    uint32_t t0, wake;

    save_setup();
    CHECK_INT(dx_service_autosave.tick(fake.ticks), 0); /* nothing changed */
    t0 = fake.ticks;
    dx_settings_set_tab(&settings, 2);
    wake = dx_service_autosave.tick(fake.ticks);
    CHECK_INT(dx_ticks_since(t0, wake), DX_SAVE_QUIET_MS);
    /* Another change 1500 ms later moves the wake. */
    fake.ticks += 1500;
    dx_items_add(&items, 0, 1);
    wake = dx_service_autosave.tick(fake.ticks);
    CHECK_INT(dx_ticks_since(t0, wake), 1500 + DX_SAVE_QUIET_MS);
    fake.ticks += 1999;
    CHECK_INT(dx_service_autosave.tick(fake.ticks), wake);
    CHECK_INT(fake.save_writes, 0);
    fake.ticks += 1;
    CHECK_INT(dx_service_autosave.tick(fake.ticks), 0);
    CHECK_INT(fake.save_writes, 2); /* both saves, once */
    CHECK(fake_find_save("DEITRIX") != NULL && fake_find_save("INV") != NULL);
    CHECK_INT(fake_find_save("DEITRIX")->size, 20);
    CHECK_INT(fake_find_save("DEITRIX")->data[12], 2);
    CHECK_INT(fake_find_save("INV")->size, 28);
    CHECK(strstr(fake.log, "deitrix: save DEITRIX.SAV ok\ndeitrix: save INV.SAV ok\n") != NULL);
    CHECK_INT(dx_autosave_pending(), 0);
}

TEST(autosave_burst_writes_once_at_the_max)
{
    uint32_t t0 = 0, wake = 0;
    int i;

    save_setup();
    for (i = 0; i < 40; i++) {
        if (i == 0) {
            t0 = fake.ticks;
        }
        dx_items_add(&items, 0, 1);
        wake = dx_service_autosave.tick(fake.ticks);
        if (fake.save_writes > 0) {
            break;
        }
        CHECK(wake != 0 && dx_ticks_since(t0, wake) <= DX_SAVE_MAX_MS);
        fake.ticks += 500;
    }
    /* A change every 500 ms: one write, 10 s after the first change. */
    CHECK_INT(fake.save_writes, 1);
    CHECK_INT(dx_ticks_since(t0, fake.ticks), DX_SAVE_MAX_MS);
    CHECK(fake_find_save("DEITRIX") == NULL);
}

TEST(autosave_flush)
{
    save_setup();
    dx_settings_set_volume(&settings, 10);
    CHECK_INT(dx_autosave_flush(fake.ticks), 0);
    CHECK_INT(fake.save_writes, 1);
    CHECK_INT(dx_service_autosave.tick(fake.ticks), 0);
    CHECK_INT(dx_autosave_flush(fake.ticks), 0); /* nothing left */
    CHECK_INT(fake.save_writes, 1);
    /* stop flushes too. */
    dx_items_toggle_marked(&items, 0);
    dx_service_autosave.stop();
    CHECK_INT(fake.save_writes, 2);
}

TEST(autosave_retry_bounded)
{
    uint32_t wake, t;

    save_setup();
    dx_settings_set_tab(&settings, 1);
    fake.save_fail = 100;
    wake = dx_service_autosave.tick(fake.ticks);
    fake.ticks += DX_SAVE_QUIET_MS;
    wake = dx_service_autosave.tick(fake.ticks);
    CHECK_INT(fake.save_writes, 1);
    CHECK(strstr(fake.log, "deitrix: save DEITRIX.SAV failed\n") != NULL);
    CHECK_INT(dx_ticks_since(fake.ticks, wake), DX_SAVE_RETRY_MS);
    CHECK_INT(dx_autosave_pending(), 1);
    /* Not before the retry time, even with a new change. */
    t = fake.ticks;
    fake.ticks += 100;
    dx_settings_set_tab(&settings, 2);
    CHECK_INT(dx_service_autosave.tick(fake.ticks), wake);
    CHECK_INT(fake.save_writes, 1);
    fake.ticks = t + DX_SAVE_RETRY_MS;
    wake = dx_service_autosave.tick(fake.ticks);
    CHECK_INT(fake.save_writes, 2);
    fake.ticks += DX_SAVE_RETRY_MS;
    wake = dx_service_autosave.tick(fake.ticks);
    fake.ticks += DX_SAVE_RETRY_MS;
    wake = dx_service_autosave.tick(fake.ticks);
    /* 1 try and 3 retries, then it gives up until the next change. */
    CHECK_INT(fake.save_writes, 4);
    CHECK_INT(wake, 0);
    CHECK_INT(dx_autosave_pending(), 0);
    CHECK(strstr(fake.log, "deitrix: save gave up\n") != NULL);
    /* The next change tries again, and a retry that works ends it. */
    fake.save_fail = 1;
    dx_settings_set_tab(&settings, 3);
    dx_service_autosave.tick(fake.ticks);
    fake.ticks += DX_SAVE_QUIET_MS;
    wake = dx_service_autosave.tick(fake.ticks);
    CHECK_INT(fake.save_writes, 5);
    fake.ticks += DX_SAVE_RETRY_MS;
    CHECK_INT(dx_service_autosave.tick(fake.ticks), 0);
    CHECK_INT(fake.save_writes, 6);
    CHECK_INT(fake_find_save("DEITRIX")->data[12], 3);
}

TEST(autosave_across_the_tick_wrap)
{
    uint32_t wake;

    save_setup();
    fake.ticks = DX_TICKS_MASK - 999; /* 1000 ms before the wrap */
    dx_settings_set_tab(&settings, 4);
    wake = dx_service_autosave.tick(fake.ticks);
    CHECK_INT(wake, 1000); /* 2000 ms later, past the wrap */
    fake.ticks = 999;
    CHECK_INT(dx_service_autosave.tick(fake.ticks), wake);
    CHECK_INT(fake.save_writes, 0);
    fake.ticks = 1000;
    CHECK_INT(dx_service_autosave.tick(fake.ticks), 0);
    CHECK_INT(fake.save_writes, 1);
}

TEST(radio_across_the_tick_wrap)
{
    setup(40000, 1);
    fake.ticks = DX_TICKS_MASK - 2000;
    dx_radio_play(1, fake.ticks);
    loop_wake = dx_service_radio.tick(fake.ticks);
    CHECK(loop_wake != 0);
    play_for(6000);
    CHECK(fake.ticks < 10000);
    CHECK_INT(dx_radio_underruns(), 0);
    CHECK_INT(fake.au_crc, stream_crc(40000, fake.au_bytes));
}
