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
/* A station starts this long after the tuner sound starts, or later, so
 * the station does not cut the sound at once. */
#define DX_RADIO_TUNING_MS 100u
/* 1 when a station that waits since start_ms may start at now_ms. */
int dx_radio_tuned(uint32_t start_ms, uint32_t now_ms);
/* As dx_radio_play, but station index i starts DX_RADIO_TUNING_MS after
 * now_ms, from the service's tick. Until then the state is
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

/* 1 while the wait before a tape goes on: a sound was started, the
 * audio is still busy, and less than DX_SFX_DRAIN_MS went by. */
int dx_sfx_wait_more(int played, int busy, uint32_t waited_ms);

/* Waits while dx_sfx_wait_more says so, through dx_plat_wait as the main
 * loop does, so the kernel keeps its services running. An event that
 * comes in the wait is dropped: Pip-OS empties the queue at a tape's
 * start anyway. */
void dx_sfx_drain(int played);

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
