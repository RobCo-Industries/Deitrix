/*
 * The two saves of docs/CONTENT.md: DEITRIX.SAV holds the settings,
 * INV.SAV the item overlay. Little-endian, a 12-byte header with a
 * CRC-32 of the payload.
 */
#ifndef DX_MODEL_SAVES_H
#define DX_MODEL_SAVES_H

#include <stdint.h>

#include "model/items.h"

#define DX_SAVE_HEADER    12
#define DX_SETTINGS_SIZE  20
#define DX_INV_RECORD     16
#define DX_INV_SAVE_MAX   (DX_SAVE_HEADER + DX_INV_RECORD * DX_ITEMS_MAX) /* 1036 */
#define DX_SETTINGS_NAME  "DEITRIX"
#define DX_INV_NAME       "INV"

#define DX_TABS           5
#define DX_VOLUME_MAX     100

enum {
    DX_SAVE_OK,
    DX_SAVE_MISSING,
    DX_SAVE_READ_ERROR,
    DX_SAVE_BAD_MAGIC,
    DX_SAVE_BAD_VERSION,
    DX_SAVE_BAD_SIZE,
    DX_SAVE_BAD_CRC,
};

typedef struct dx_settings {
    uint8_t last_tab;     /* 0 to 4 */
    uint8_t volume;       /* 0 to 100 */
    uint8_t ui_sound;     /* 0 off, 1 on, 2 full */
    uint8_t last_station; /* 1 to 6, or 0 for none */
    uint32_t changes;     /* counts changes, for the saves */
} dx_settings_t;

/* The defaults: tab 0, `volume`, UI sound on, no station. */
void dx_settings_defaults(dx_settings_t *s, uint8_t volume);

/* Setters: 1 when the value changed, and then they count a change. A
 * value out of range does nothing and returns 0. */
int dx_settings_set_tab(dx_settings_t *s, int tab);
int dx_settings_set_volume(dx_settings_t *s, int volume);
int dx_settings_set_ui_sound(dx_settings_t *s, int level);
int dx_settings_set_station(dx_settings_t *s, int number);

/* Writes the 20 bytes of DEITRIX.SAV. */
void dx_settings_encode(const dx_settings_t *s, uint8_t out[DX_SETTINGS_SIZE]);

/*
 * Reads DEITRIX.SAV from n bytes, or from the dx_plat_save_read error in
 * n. A bad save leaves the defaults; a field out of range takes its
 * default. Returns a DX_SAVE_*. `changes` is left as it was.
 */
int dx_settings_decode(dx_settings_t *s, const uint8_t *buf, int32_t n, uint8_t volume);

/* Writes INV.SAV for every item in file order. Returns its size. */
int dx_inv_encode(const dx_items_t *it, uint8_t out[DX_INV_SAVE_MAX]);

/*
 * Applies INV.SAV from n bytes, or the dx_plat_save_read error in n, to
 * the overlay. Every item first takes its ITEMS.INI count, not marked.
 * A bad save changes nothing else. A record whose id is not an item is
 * dropped, the first of two with the same id wins, and a field out of
 * range keeps the item's default. `changes` is left as it was.
 * Returns a DX_SAVE_*; *applied gets the records used.
 */
int dx_inv_decode(dx_items_t *it, const uint8_t *buf, int32_t n, int *applied);

/* "ok", "missing" or "ignored: <reason>". */
const char *dx_save_reason(int status);

#endif
