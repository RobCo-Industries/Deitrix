/*
 * The background services of the app shell: the radio, which keeps
 * playing whatever tab is shown, the autosave, the uptime and the tape
 * slot.
 */
#ifndef DX_SERVICES_SERVICES_H
#define DX_SERVICES_SERVICES_H

#include <stdint.h>

#include "app.h"
#include "model/items.h"
#include "model/radio.h"
#include "model/saves.h"
#include "services/sfx_table.h"
#include "ui/notice.h"

/* ---- Radio ------------------------------------------------------------- */

/* DX_RADIO_TUNING: the tuner sound plays and a station waits to start. */
enum { DX_RADIO_OFF, DX_RADIO_TUNE, DX_RADIO_PCM, DX_RADIO_TUNING };

/* A PCM track streams in chunks of 8000 bytes: 1000 ms each at 8000 Hz. */
#define DX_RADIO_CHUNK    8000u
#define DX_RADIO_CHUNK_MS 1000u
/* While a track streams the service wakes every 250 ms, a quarter of a
 * chunk: the queue then has at least 750 ms of sound left. */
#define DX_RADIO_POLL_MS  250u
/* After a tune's length, the check that it ended repeats this often. */
#define DX_RADIO_TUNE_POLL_MS 50u

extern const dx_service_t dx_service_radio;

/* The stations the radio plays. Call before the services start. */
void dx_radio_attach(const dx_radio_t *stations);

/* Starts station index i of the table: 0, or -1 when it cannot play.
 * Whatever played stops first. */
int dx_radio_play(int i, uint32_t now_ms);
void dx_radio_stop(void);
/* A station starts at least this long after the tuner sound was asked
 * for, and once the audio status reads idle, so it does not cut the
 * sound. */
#define DX_RADIO_TUNING_MS 100u
/* This long after the request the station starts whatever the status
 * says, so a stuck status cannot keep the radio silent. */
#define DX_RADIO_TUNING_MAX_MS 300u
/* While tuning the tick checks the status this often. */
#define DX_RADIO_TUNING_POLL_MS 10u
/* 1 when a station that waits since start_ms may start at now_ms. idle
 * is 1 when the audio status reads idle. */
int dx_radio_tuned(uint32_t start_ms, uint32_t now_ms, int idle);
/* As dx_radio_play, but station index i starts later, as
 * dx_radio_tuned says, from the service's tick. Until then the state is
 * DX_RADIO_TUNING, the station counts as current and UI sounds stay
 * blocked. 0, or -1 when it cannot play. */
int dx_radio_tune(int i, uint32_t now_ms);
/* The station index that plays, or -1. */
int dx_radio_current(void);
int dx_radio_state(void);
/* Sets the audio level, 0 to 100. 0, or -1 for a bad level. */
int dx_radio_set_volume(int level);
/* How many times the stream ran dry and restarted. */
uint32_t dx_radio_underruns(void);
/* The station index an error stopped: its file could not be opened,
 * read or played. -1 once a station plays or dx_radio_stop runs. */
int dx_radio_failed(void);

/* ---- Autosave ---------------------------------------------------------- */

/* A save waits for 2 s with no change, and at most 10 s after the first
 * change. A failed write is tried again 5 s later, at most 3 times.
 * Then the service stops trying until the next change or flush. */
#define DX_SAVE_QUIET_MS 2000u
#define DX_SAVE_MAX_MS   10000u
#define DX_SAVE_RETRY_MS 5000u
#define DX_SAVE_RETRIES  3
/* Changes not on the card this long show the unsaved mark: past the
 * longest wait of a healthy save, so it never shows on one. */
#define DX_SAVE_MARK_MS  (DX_SAVE_MAX_MS + DX_SAVE_QUIET_MS)

extern const dx_service_t dx_service_autosave;

/* The model the autosave watches. Call before the services start. */
void dx_autosave_attach(dx_settings_t *settings, dx_items_t *items);

/* Tries once to write every save not on the card, even after the retries
 * gave up. Returns 0 when every save is on the card. */
int dx_autosave_flush(uint32_t now_ms);

/* Counts one change of the device's sound levels, the master included.
 * The service then stores them with dx_plat_levels_store under the rules
 * of a save. Does nothing before ABI 1.6. */
void dx_autosave_note_levels(void);

/* 1 while the service plans to write a change. 0 after it gave up. */
int dx_autosave_pending(void);

/* 1 while a change is not on the card, even after the retries gave up. */
int dx_autosave_unsaved(void);

/* 1 while the header shows the unsaved mark: a change has not reached
 * the card for DX_SAVE_MARK_MS. The service's tick turns it on and off,
 * logs "deitrix: unsaved mark on" or "off", and asks for a redraw. */
int dx_autosave_marked(void);

/* ---- Uptime ------------------------------------------------------------ */

/* The service samples ticks_ms this often at least: far less than the
 * 24.8 days between two wraps. */
#define DX_UPTIME_SAMPLE_MS 3600000u

extern const dx_service_t dx_service_uptime;

/* The whole seconds since the start, past the wraps of ticks_ms at 2^31,
 * and the ms into the current second in *ms_in_s when not NULL. Samples
 * now_ms, as the service does. */
uint32_t dx_uptime_s(uint32_t now_ms, uint32_t *ms_in_s);

/* Starts the count again at now_ms with no wrap, for the host tests. */
void dx_uptime_reset(uint32_t now_ms);

/* ---- Sound effects ------------------------------------------------------ */

/*
 * 1 when the weep may play: UI sounds on, the radio not holding the
 * voice, ABI 1.5 or later for sample_rate, and hw_flags with DX_HW_AUDIO
 * and DX_HW_AUDIO_MID. With the output resting low a sample starts and
 * ends with a thump, so the weep stays silent there.
 */
int dx_sfx_weep_allowed(int ui_sound_on, int radio_on, uint32_t abi_version, uint32_t hw_flags);

/*
 * Plays the weep, a short rising chirp, at DX_RATE_HI, then sets the
 * rate back to the one before. Does nothing when dx_sfx_weep_allowed
 * says no. Returns 1 when it started the weep, else 0. The shell calls
 * it once, after the first frame.
 */
int dx_sfx_weep(void);

/* 1 when a table may play on this kernel: ABI 1.5 or later, and
 * hw_flags with DX_HW_AUDIO and DX_HW_AUDIO_MID. */
int dx_sfx_mid(uint32_t abi_version, uint32_t hw_flags);

/* dx_sfx_mid for the running kernel. The "UI sounds" row offers "Full"
 * only when it is 1. */
int dx_sfx_full(void);

/* The tables of src/services/sfx_table.c that dx_sfx_play knows. */
enum {
    DX_SFX_ID_SCROLL,
    DX_SFX_ID_SELECT,
    DX_SFX_ID_MODE,
    DX_SFX_ID_TAB,
    DX_SFX_ID_HIGHLIGHT,
    DX_SFX_ID_TUNER,
    DX_SFX_ID_TAPE_START,
    DX_SFX_ID_TAPE_STOP,
    DX_SFX_COUNT
};

/*
 * Starts table `id` at DX_RATE_HI, then sets the rate back to the one
 * before, as the weep does. It never waits for the play. Does nothing
 * on a kernel where dx_sfx_full says no, or while the radio plays.
 * Returns 1 when the play started, else 0.
 */
int dx_sfx_play(int id);

/* The longest wait for a sound to end before a tape starts. */
#define DX_SFX_DRAIN_MS 500u

/* The most rounds of the wait, in case the clock stands still. A key
 * ends a round at once, so the cap is far above 500 ms of 10 ms rounds:
 * the clock, not the count, ends the wait. */
#define DX_SFX_DRAIN_ROUNDS 5000u

/* 1 while the wait before a tape goes on: a sound was started, the
 * audio is still busy, less than DX_SFX_DRAIN_MS went by on the clock,
 * and fewer than DX_SFX_DRAIN_ROUNDS rounds ran. */
int dx_sfx_wait_more(int played, int busy, uint32_t waited_ms, unsigned rounds);

/* Waits while dx_sfx_wait_more says so, through dx_plat_wait as the main
 * loop does, so the kernel keeps its services running. An event that
 * comes in the wait is dropped: Pip-OS empties the queue at a tape's
 * start anyway. */
void dx_sfx_drain(int played);

/* ---- The hum ------------------------------------------------------------ */

/*
 * The hum is a quiet loop under the other UI sounds, on "Full + hum".
 * It plays as two parts that take turns in the kernel's queue. A sound
 * over the hum is a head, the sound mixed with the hum's first bytes,
 * with the rest of the hum's first part queued behind it. The heads are
 * mixed once, into RAM, when the service starts.
 */

/* The service tops the queue up this often. */
#define DX_HUM_POLL_MS 250u
/* A full panel frame blocks the ROM this long. The rest behind the
 * longest head must outlast it plus one top-up period. */
#define DX_HUM_FRAME_MS 128u

/* The bytes of every head, one after the other. */
#define DX_SFX_HEADS_SIZE                                                                       \
    (DX_SFX_SCROLL_HEAD_LEN + DX_SFX_SELECT_HEAD_LEN + DX_SFX_MODE_HEAD_LEN +                   \
     DX_SFX_TAB_HEAD_LEN + DX_SFX_HIGHLIGHT_HEAD_LEN + DX_SFX_TUNER_HEAD_LEN +                  \
     DX_SFX_TAPE_START_HEAD_LEN + DX_SFX_TAPE_STOP_HEAD_LEN)

/* out[i] = clamp(shot[i] + bed[i] - 128, 0, 255) for i below n. */
void dx_sfx_mix(uint8_t *out, const uint8_t *shot, const uint8_t *bed, uint32_t n);

/* The head of table `id` and its length, mixed at the service's start,
 * or NULL for a bad id. */
const uint8_t *dx_sfx_head(int id, uint32_t *len);

/* The state of the hum, apart from the kernel. */
typedef struct dx_hum {
    int running; /* the hum plays, or waits in the queue */
    int next;    /* the part to queue next: 0 or 1 */
} dx_hum_t;

/* What a top-up asks for. START: part one now. QUEUE: part `next`
 * behind what plays. STOP: stop the sound. */
enum { DX_HUM_NONE, DX_HUM_START, DX_HUM_QUEUE, DX_HUM_STOP };

/*
 * One top-up. `on`: the setting is "Full + hum" and tables may play.
 * `radio`: the radio is not off. `busy`: the audio status. Off or with
 * the radio, the hum stops: STOP only when the hum ran and the radio
 * does not hold the voice. A hum that runs starts again when nothing
 * plays, else queues its next part. A hum that does not run waits for
 * the other sound to end, then starts.
 */
int dx_hum_step(dx_hum_t *h, int on, int radio, int busy);

/* 1 when "Full + hum" is set and tables may play. */
int dx_hum_on(void);

/* 1 while the hum runs, so a table plays as its head. */
int dx_hum_running(void);

/* Another sound took the voice: the hum waits for its end. The tones,
 * the weep, the plain tables, a station and a tape call it. */
void dx_hum_halt(void);

/*
 * The setting left "Full + hum". The hum stops at once and waits for
 * the setting to come back. STOP only when the hum ran and a sound
 * plays: what plays then is the hum, or a head with the hum's rest
 * queued behind it, since every other sound halts the hum first. A
 * head is part hum, so the stop cuts it too.
 */
int dx_hum_leave(dx_hum_t *h, int busy);

/* dx_hum_leave on the service's state, and the stop it asks for. The
 * setting calls it in the same pass as the change. */
void dx_hum_setting_left(void);

extern const dx_service_t dx_service_hum;

/* The hum's state, for the host tests. */
dx_hum_t *dx_hum_state(void);

/* ---- Tape slot ---------------------------------------------------------- */

/* The footer notices of the slot changes. */
#define DX_TAPES_IN  "Tape slot: card loaded."
#define DX_TAPES_OUT "Tape slot: card removed."
#define DX_TAPES_BAD "Tape slot: card unreadable."

extern const dx_service_t dx_service_tapes;

/* 1 when the kernel reports a slot: ABI 1.4 or later and DX_HW_TAPE.
 * With 0 nothing about a slot shows anywhere. */
int dx_tapes_slot(void);

/* Counts the slot changes seen: the TAPES list reads the holotapes
 * again when it moved. */
uint32_t dx_tapes_gen(void);

/* Asks Pip-OS for the slot changes now, as after a launch, and moves
 * the notices on. Returns the DX_HOLO_EV_* bits, 0 without a slot. */
uint32_t dx_tapes_poll(uint32_t now_ms);

/* The notice the footer shows, or NULL. */
const char *dx_tapes_notice(void);

/* Back to the state before init, for the host tests. */
void dx_tapes_reset(void);

#endif
