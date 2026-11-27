/*
 * The radio service. A tune goes to play_seq in one call and starts
 * again when the sound ends. A PCM track streams from its file: one
 * chunk plays, one waits in the kernel, one waits in chunk[], and the
 * service tops the queue up every DX_RADIO_POLL_MS. Every chunk but the
 * last of a track that does not loop is DX_RADIO_CHUNK bytes long.
 * With the tuner sound a station waits in DX_RADIO_TUNING first, and
 * the tick starts it DX_RADIO_TUNING_MS after the sound's start.
 */
#include "services/services.h"

#include "platform/platform.h"
#include "ui/ui.h"

#define PATH_MAX_LEN 48

static const dx_radio_t *table;
static int state = DX_RADIO_OFF;
static int current = -1;
static int32_t fd = -1;
static uint8_t chunk[DX_RADIO_CHUNK];
static uint32_t pending;  /* bytes in chunk not handed to the kernel */
static int started;       /* a chunk of this stream reached the kernel */
static int at_end;        /* loop = no and the file is done */
static uint32_t underruns;
static int failed = -1;   /* the station an error stopped, or -1 */
static uint32_t tuned_at; /* DX_RADIO_TUNING: when the tuner sound started */

void dx_radio_attach(const dx_radio_t *stations)
{
    table = stations;
}

int dx_radio_current(void)
{
    return current;
}

int dx_radio_state(void)
{
    return state;
}

uint32_t dx_radio_underruns(void)
{
    return underruns;
}

int dx_radio_failed(void)
{
    return failed;
}

int dx_radio_set_volume(int level)
{
    if (level < 0 || level > 100) {
        return -1;
    }
    return dx_plat_volume((uint32_t)level) < 0 ? -1 : 0;
}

static void close_file(void)
{
    if (fd >= 0) {
        dx_plat_file_close(fd);
        fd = -1;
    }
}

/* Back to off without touching the sound. */
static void off(void)
{
    close_file();
    state = DX_RADIO_OFF;
    current = -1;
    pending = 0;
    dx_ui_sound_block(0);
}

void dx_radio_stop(void)
{
    /* While tuning only the tuner sound plays: a stop leaves it be. */
    if (state != DX_RADIO_OFF && state != DX_RADIO_TUNING) {
        dx_plat_audio_stop();
    }
    off();
    failed = -1;
}

/* Stops on an error and keeps which station it was. */
static void fail(void)
{
    int was = current;

    dx_radio_stop();
    failed = was;
}

static int open_track(void)
{
    char p[PATH_MAX_LEN];
    dx_sb_t sb;

    dx_sb_init(&sb, p, sizeof(p));
    dx_sb_str(&sb, "/PIPBOY/DATA/RADIO/");
    dx_sb_str(&sb, table->stations[current].file);
    fd = dx_plat_file_open(p);
    return fd >= 0;
}

/*
 * Fills chunk[] from the file. At the end of a looping track the file
 * opens again and the chunk fills on from its first byte, so every
 * chunk is whole and the join plays with no gap. Returns 1 with pending
 * set, 0 at the end of a track that does not loop, -1 on an error.
 */
static int read_chunk(void)
{
    uint32_t got = 0;
    int reopened = 0;

    while (got < DX_RADIO_CHUNK) {
        int32_t n = dx_plat_file_read(fd, chunk + got, DX_RADIO_CHUNK - got);

        if (n < 0) {
            return -1;
        }
        if (n > 0) {
            got += (uint32_t)n;
            reopened = 0;
            continue;
        }
        if (!table->stations[current].loop) {
            at_end = 1;
            break;
        }
        if (reopened) {
            return -1; /* the file is empty */
        }
        close_file();
        if (!open_track()) {
            return -1;
        }
        reopened = 1;
    }
    pending = got;
    return got > 0 ? 1 : 0;
}

/* Hands chunks to the kernel until it has one waiting. */
static void feed(void)
{
    int k;

    for (k = 0; k < 3 && state == DX_RADIO_PCM; k++) {
        int32_t rc;

        if (pending == 0) {
            int r = at_end ? 0 : read_chunk();

            if (r < 0) {
                fail();
                return;
            }
            if (r == 0) {
                return;
            }
        }
        if (started && !dx_plat_audio_status()) {
            /* The queue ran dry: this chunk starts the sound again. */
            underruns++;
            dx_plat_log("deitrix: radio underrun\n");
        }
        rc = started ? dx_plat_sample_queue(chunk, pending) : dx_plat_sample(chunk, pending);
        if (rc == DX_EBUSY) {
            return;
        }
        if (rc != 0) {
            fail();
            return;
        }
        started = 1;
        pending = 0;
    }
}

/* 1 when station i can start, else 0 with failed set. */
static int can_play(int i)
{
    if (table == NULL || i < 0 || i >= table->count) {
        return 0;
    }
    if (!table->stations[i].playable) {
        failed = i;
        return 0;
    }
    return 1;
}

/* Starts station i, which can_play allowed, from off. */
static int start(int i)
{
    const dx_station_t *s = &table->stations[i];

    current = i;
    started = 0;
    at_end = 0;
    pending = 0;
    if (s->kind == DX_KIND_TUNE) {
        if (dx_plat_play_seq(s->notes, (uint32_t)s->note_count) != 0) {
            off();
            failed = i;
            return -1;
        }
        state = DX_RADIO_TUNE;
    } else {
        if (!open_track()) {
            off();
            failed = i;
            return -1;
        }
        state = DX_RADIO_PCM;
        feed();
        if (state != DX_RADIO_PCM) {
            return -1;
        }
    }
    dx_ui_sound_block(1);
    return 0;
}

int dx_radio_play(int i, uint32_t now_ms)
{
    (void)now_ms;
    dx_radio_stop();
    return can_play(i) ? start(i) : -1;
}

int dx_radio_tuned(uint32_t start_ms, uint32_t now_ms)
{
    return dx_ticks_since(start_ms, now_ms) >= DX_RADIO_TUNING_MS;
}

int dx_radio_tune(int i, uint32_t now_ms)
{
    dx_radio_stop();
    if (!can_play(i)) {
        return -1;
    }
    current = i;
    tuned_at = now_ms;
    state = DX_RADIO_TUNING;
    dx_ui_sound_block(1);
    return 0;
}

static uint32_t step(uint32_t now_ms)
{
    const dx_station_t *s;

    switch (state) {
    case DX_RADIO_TUNING:
        if (!dx_radio_tuned(tuned_at, now_ms)) {
            return dx_ticks_add(tuned_at, DX_RADIO_TUNING_MS);
        }
        /* The tuner sound has had its time: no stop, the play replaces it. */
        state = DX_RADIO_OFF;
        if (start(current) != 0) {
            return 0;
        }
        return dx_ticks_add(now_ms, state == DX_RADIO_PCM ? DX_RADIO_POLL_MS
                                                          : DX_RADIO_TUNE_POLL_MS);
    case DX_RADIO_TUNE:
        s = &table->stations[current];
        if (dx_plat_audio_status()) {
            return dx_ticks_add(now_ms, DX_RADIO_TUNE_POLL_MS);
        }
        /* The tune ended: it plays again from its start. */
        if (dx_plat_play_seq(s->notes, (uint32_t)s->note_count) != 0) {
            failed = current;
            off();
            return 0;
        }
        return dx_ticks_add(now_ms, s->tune_ms);
    case DX_RADIO_PCM:
        feed();
        if (state == DX_RADIO_PCM && at_end && pending == 0 && !dx_plat_audio_status()) {
            off(); /* a track that does not loop has ended */
        }
        return state == DX_RADIO_PCM ? dx_ticks_add(now_ms, DX_RADIO_POLL_MS) : 0;
    default:
        return 0;
    }
}

/* A screen shows the state, the station and may show the underruns: a
 * change of any asks for a redraw. A tune's replay and a chunk handed
 * to the kernel change none of them. */
static uint32_t tick(uint32_t now_ms, unsigned *flags)
{
    int was_state = state;
    int was_current = current;
    uint32_t was_underruns = underruns;
    uint32_t next = step(now_ms);

    if (state != was_state || current != was_current || underruns != was_underruns) {
        *flags |= DX_UI_DIRTY;
    }
    return next;
}

static void init(uint32_t now_ms)
{
    (void)now_ms;
    off();
    underruns = 0;
    failed = -1;
}

const dx_service_t dx_service_radio = { "radio", init, tick, dx_radio_stop };
