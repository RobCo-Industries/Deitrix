/*
 * The radio service. A tune goes to play_seq in one call and starts
 * again when the sound ends. A PCM track streams from its file: one
 * chunk plays, one waits in the kernel, one waits in chunk[], and the
 * service tops the queue up every DX_RADIO_POLL_MS. Every chunk but the
 * last of a track that does not loop is DX_RADIO_CHUNK bytes long.
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
    if (state != DX_RADIO_OFF) {
        dx_plat_audio_stop();
    }
    off();
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
                dx_radio_stop();
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
            dx_radio_stop();
            return;
        }
        started = 1;
        pending = 0;
    }
}

int dx_radio_play(int i, uint32_t now_ms)
{
    const dx_station_t *s;

    (void)now_ms;
    dx_radio_stop();
    if (table == NULL || i < 0 || i >= table->count || !table->stations[i].playable) {
        return -1;
    }
    s = &table->stations[i];
    current = i;
    started = 0;
    at_end = 0;
    pending = 0;
    if (s->kind == DX_KIND_TUNE) {
        if (dx_plat_play_seq(s->notes, (uint32_t)s->note_count) != 0) {
            off();
            return -1;
        }
        state = DX_RADIO_TUNE;
    } else {
        if (!open_track()) {
            off();
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

static uint32_t tick(uint32_t now_ms)
{
    const dx_station_t *s;

    switch (state) {
    case DX_RADIO_TUNE:
        s = &table->stations[current];
        if (dx_plat_audio_status()) {
            return dx_ticks_add(now_ms, DX_RADIO_TUNE_POLL_MS);
        }
        /* The tune ended: it plays again from its start. */
        if (dx_plat_play_seq(s->notes, (uint32_t)s->note_count) != 0) {
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

static void init(uint32_t now_ms)
{
    (void)now_ms;
    off();
    underruns = 0;
}

const dx_service_t dx_service_radio = { "radio", init, tick, dx_radio_stop };
