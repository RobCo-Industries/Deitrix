/*
 * The loaded content and the saves: the one copy of the model the ROM
 * keeps. dx_content_load reads every file under /PIPBOY/DATA once at
 * start, dx_content_load_saves applies the saves on top. Screens read
 * through the accessors; changes go through the model's functions on
 * dx_content_items() and dx_content_settings(), and the autosave
 * service writes them.
 */
#ifndef DX_CONTENT_H
#define DX_CONTENT_H

#include <stdint.h>

#include "model/items.h"
#include "model/map.h"
#include "model/notes.h"
#include "model/profile.h"
#include "model/radio.h"
#include "model/saves.h"

#define DX_DATA_DIR "/PIPBOY/DATA"

/*
 * Loads PROFILE.INI, ITEMS.INI, NOTES, MAP.INI, RADIO.INI and the
 * station files. Each is independent: a bad one does not stop the
 * others. Logs one "deitrix: load <FILE> ..." line per file.
 */
void dx_content_load(void);

/*
 * Reads DEITRIX.SAV and INV.SAV and applies them. The volume default is
 * the level Pip-OS had: `volume`. Logs one line per save. Then sets the
 * volume and the UI sound from the settings.
 */
void dx_content_load_saves(uint8_t volume);

const dx_profile_t *dx_content_profile(void);
const dx_notes_t *dx_content_notes(void);
const dx_maps_t *dx_content_maps(void);
const dx_radio_t *dx_content_radio(void);
dx_items_t *dx_content_items(void);
dx_settings_t *dx_content_settings(void);
/* The save status of DEITRIX.SAV and INV.SAV: DX_SAVE_*. */
int dx_content_settings_status(void);
int dx_content_inv_status(void);

/* Reads note i's body into out, as dx_notes_body does. Returns its
 * length, or -1 when the file cannot be read. */
int dx_content_note_body(int i, char *out, int cap);

#endif
