/*
 * Sound effects that are samples, not UI tones. The weep is a 140 ms
 * chirp at 37500 Hz from src/services/weep_table.c. The other sounds
 * come from src/services/sfx_table.c, at the same rate. Each needs
 * sample_rate, new in ABI 1.5, and an output that rests at its
 * midpoint, or the play would thump at both ends. On "Full + hum" a
 * service keeps the hum going under the other sounds.
 */
#include "services/services.h"

#include "platform/platform.h"
#include "services/sfx_table.h"
#include "services/weep_table.h"
#include "ui/ui.h"

_Static_assert(DX_WEEP_RATE == DX_RATE_HI, "the weep's rate");
_Static_assert(DX_SFX_RATE_HZ == DX_RATE_HI, "the tables' rate");

int dx_sfx_mid(uint32_t abi_version, uint32_t hw_flags)
{
    const uint32_t need = DX_HW_AUDIO | DX_HW_AUDIO_MID;

    return abi_version >= DX_ABI_1_5 && (hw_flags & need) == need;
}

int dx_sfx_weep_allowed(int ui_sound_on, int radio_on, uint32_t abi_version, uint32_t hw_flags)
{
    return ui_sound_on && !radio_on && dx_sfx_mid(abi_version, hw_flags);
}

int dx_sfx_full(void)
{
    dx_sysinfo_t si;

    dx_plat_sysinfo(&si); /* zeroed on failure: no ABI, no flags */
    return dx_sfx_mid(si.abi_version, si.hw_flags);
}

/* The tables dx_sfx_play knows, by DX_SFX_ID_*. */
static const struct {
    const uint8_t *data;
    uint32_t len;
} tables[DX_SFX_COUNT] = {
    [DX_SFX_ID_SCROLL] = { dx_sfx_scroll, DX_SFX_SCROLL_LEN },
    [DX_SFX_ID_SELECT] = { dx_sfx_select, DX_SFX_SELECT_LEN },
    [DX_SFX_ID_MODE] = { dx_sfx_mode, DX_SFX_MODE_LEN },
    [DX_SFX_ID_TAB] = { dx_sfx_tab, DX_SFX_TAB_LEN },
    [DX_SFX_ID_HIGHLIGHT] = { dx_sfx_highlight, DX_SFX_HIGHLIGHT_LEN },
    [DX_SFX_ID_TUNER] = { dx_sfx_tuner, DX_SFX_TUNER_LEN },
    [DX_SFX_ID_TAPE_START] = { dx_sfx_tape_start, DX_SFX_TAPE_START_LEN },
    [DX_SFX_ID_TAPE_STOP] = { dx_sfx_tape_stop, DX_SFX_TAPE_STOP_LEN },
};

/* Each head is as long as its table, and shorter than the hum's first
 * part, so its rest is never empty. */
_Static_assert(DX_SFX_SCROLL_HEAD_LEN == DX_SFX_SCROLL_LEN, "scroll head");
_Static_assert(DX_SFX_SELECT_HEAD_LEN == DX_SFX_SELECT_LEN, "select head");
_Static_assert(DX_SFX_MODE_HEAD_LEN == DX_SFX_MODE_LEN, "mode head");
_Static_assert(DX_SFX_TAB_HEAD_LEN == DX_SFX_TAB_LEN, "tab head");
_Static_assert(DX_SFX_HIGHLIGHT_HEAD_LEN == DX_SFX_HIGHLIGHT_LEN, "highlight head");
_Static_assert(DX_SFX_TUNER_HEAD_LEN == DX_SFX_TUNER_LEN, "tuner head");
_Static_assert(DX_SFX_TAPE_START_HEAD_LEN == DX_SFX_TAPE_START_LEN, "tape_start head");
_Static_assert(DX_SFX_TAPE_STOP_HEAD_LEN == DX_SFX_TAPE_STOP_LEN, "tape_stop head");
_Static_assert(DX_SFX_BED_HEAD_MAX < DX_SFX_HUM_1_LEN, "every rest is longer than 0");

/* The heads, mixed once when the hum service starts. */
static uint8_t heads[DX_SFX_HEADS_SIZE];
static uint32_t head_at[DX_SFX_COUNT];
static int mixed;

/* The hum's two parts. */
static const uint8_t *const parts[2] = { dx_sfx_hum_1, dx_sfx_hum_2 };
static const uint32_t part_len[2] = { DX_SFX_HUM_1_LEN, DX_SFX_HUM_2_LEN };

static dx_hum_t hum;

void dx_sfx_mix(uint8_t *out, const uint8_t *shot, const uint8_t *bed, uint32_t n)
{
    uint32_t i;

    for (i = 0; i < n; i++) {
        int v = (int)shot[i] + (int)bed[i] - 128;

        out[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
}

static void mix_heads(void)
{
    uint32_t at = 0;
    int id;

    if (mixed) {
        return;
    }
    for (id = 0; id < DX_SFX_COUNT; id++) {
        head_at[id] = at;
        dx_sfx_mix(heads + at, tables[id].data, dx_sfx_hum_1, tables[id].len);
        at += tables[id].len;
    }
    mixed = 1;
}

const uint8_t *dx_sfx_head(int id, uint32_t *len)
{
    if (id < 0 || id >= DX_SFX_COUNT) {
        return NULL;
    }
    mix_heads();
    *len = tables[id].len;
    return heads + head_at[id];
}

/* Plays len bytes of data at DX_RATE_HI, then queues rest_len bytes of
 * rest behind it when rest is not NULL, then sets the rate back to the
 * one before. Each play keeps the rate it started with, and a queued
 * part keeps the rate of the play it joins, so a radio track that
 * starts later gets its own rate. With queue set the data goes to
 * sample_queue. 1 when the data went to the kernel. */
static int play_hi(const uint8_t *data, uint32_t len, const uint8_t *rest, uint32_t rest_len,
                   int queue)
{
    int32_t prev;
    int32_t rc;

    prev = dx_plat_sample_rate(DX_RATE_HI);
    if (prev < 0) {
        return 0;
    }
    rc = queue ? dx_plat_sample_queue(DX_SOUND_SFX, data, len)
               : dx_plat_sample(DX_SOUND_SFX, data, len);
    if (rc == 0 && rest != NULL) {
        dx_plat_sample_queue(DX_SOUND_SFX, rest, rest_len); /* nothing waits: it never says busy */
    }
    dx_plat_sample_rate((uint32_t)prev);
    return rc == 0;
}

/* A plain play: it takes the voice from the hum. */
static int play_plain(const uint8_t *data, uint32_t len)
{
    int ok = play_hi(data, len, NULL, 0, 0);

    if (ok) {
        dx_hum_halt();
    }
    return ok;
}

int dx_sfx_play(int id)
{
    if (id < 0 || id >= DX_SFX_COUNT || dx_radio_state() != DX_RADIO_OFF || !dx_sfx_full()) {
        return 0;
    }
    if (hum.running && dx_ui_sound_hum()) {
        uint32_t n;
        const uint8_t *head = dx_sfx_head(id, &n);

        if (play_hi(head, n, dx_sfx_hum_1 + n, DX_SFX_HUM_1_LEN - n, 0)) {
            hum.next = 1; /* the rest of part one waits: part two comes next */
            return 1;
        }
        return 0;
    }
    return play_plain(tables[id].data, tables[id].len);
}

int dx_sfx_wait_more(int played, int busy, uint32_t waited_ms, unsigned rounds)
{
    return played && busy && waited_ms < DX_SFX_DRAIN_MS && rounds < DX_SFX_DRAIN_ROUNDS;
}

/* One wait at most this long: a short step keeps the end close. */
#define DRAIN_STEP_MS 10u

void dx_sfx_drain(int played)
{
    uint32_t start = dx_plat_ticks();
    uint32_t waited = 0;
    unsigned n;

    for (n = 0; dx_sfx_wait_more(played, dx_plat_audio_status(), waited, n); n++) {
        uint32_t left = DX_SFX_DRAIN_MS - waited;
        dx_event_t ev;

        dx_plat_wait(&ev, left < DRAIN_STEP_MS ? left : DRAIN_STEP_MS);
        waited = dx_ticks_since(start, dx_plat_ticks());
    }
}

int dx_sfx_weep(void)
{
    dx_sysinfo_t si;

    dx_plat_sysinfo(&si); /* zeroed on failure: no ABI, no flags */
    if (!dx_sfx_weep_allowed(dx_ui_sound_enabled(), dx_radio_state() != DX_RADIO_OFF,
                             si.abi_version, si.hw_flags)) {
        return 0;
    }
    return play_plain(dx_weep, DX_WEEP_LEN);
}

/* ---- The hum service ------------------------------------------------------ */

int dx_hum_step(dx_hum_t *h, int on, int radio, int busy)
{
    if (!on || radio) {
        int was = h->running;

        h->running = 0;
        return was && !radio ? DX_HUM_STOP : DX_HUM_NONE;
    }
    if (!busy) {
        h->running = 1;
        h->next = 1;
        return DX_HUM_START;
    }
    return h->running ? DX_HUM_QUEUE : DX_HUM_NONE;
}

int dx_hum_leave(dx_hum_t *h, int busy)
{
    int was = h->running;

    h->running = 0;
    h->next = 0;
    return was && busy ? DX_HUM_STOP : DX_HUM_NONE;
}

void dx_hum_setting_left(void)
{
    if (dx_hum_leave(&hum, dx_plat_audio_status()) == DX_HUM_STOP) {
        dx_plat_audio_stop();
    }
}

int dx_hum_on(void)
{
    return dx_ui_sound_hum() && dx_sfx_full();
}

int dx_hum_running(void)
{
    return hum.running;
}

void dx_hum_halt(void)
{
    hum.running = 0;
}

dx_hum_t *dx_hum_state(void)
{
    return &hum;
}

/* The next top-up. A key wakes every service at once: the hum keeps
 * its own period, so a tone after a key plays out. */
static uint32_t due;

static void hum_init(uint32_t now_ms)
{
    mix_heads();
    hum.running = 0;
    hum.next = 0;
    /* The first top-up comes one period late, after the weep. */
    due = dx_ticks_add(now_ms, DX_HUM_POLL_MS);
}

static uint32_t hum_tick(uint32_t now_ms, unsigned *flags)
{
    (void)flags;
    if (!dx_ticks_due(now_ms, due)) {
        return due;
    }
    due = dx_ticks_add(now_ms, DX_HUM_POLL_MS);
    switch (dx_hum_step(&hum, dx_hum_on(), dx_radio_state() != DX_RADIO_OFF,
                        dx_plat_audio_status())) {
    case DX_HUM_START:
        if (!play_hi(parts[0], part_len[0], NULL, 0, 0)) {
            hum.running = 0;
        }
        break;
    case DX_HUM_QUEUE:
        /* Busy means the queue already holds a part: the next try comes
         * one period later. */
        if (play_hi(parts[hum.next], part_len[hum.next], NULL, 0, 1)) {
            hum.next ^= 1;
        }
        break;
    case DX_HUM_STOP:
        dx_plat_audio_stop();
        break;
    default:
        break;
    }
    return due;
}

static void hum_stop(void)
{
    hum.running = 0;
}

const dx_service_t dx_service_hum = { "hum", hum_init, hum_tick, hum_stop };
