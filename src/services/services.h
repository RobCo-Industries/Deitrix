/*
 * The background services of the app shell: the radio, which keeps
 * playing whatever tab is shown, and the autosave.
 */
#ifndef DX_SERVICES_SERVICES_H
#define DX_SERVICES_SERVICES_H

#include <stdint.h>

#include "app.h"
#include "model/items.h"
#include "model/radio.h"
#include "model/saves.h"

/* ---- Radio ------------------------------------------------------------- */

enum { DX_RADIO_OFF, DX_RADIO_TUNE, DX_RADIO_PCM };

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

#endif
