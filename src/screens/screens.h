/*
 * The ROM's screens, one per tab, and the table the app shell runs.
 * Each screen file defines one dx_screen_t.
 */
#ifndef DX_SCREENS_SCREENS_H
#define DX_SCREENS_SCREENS_H

#include "ui/ui.h"

extern const dx_screen_t dx_screen_stat;
extern const dx_screen_t dx_screen_inv;
extern const dx_screen_t dx_screen_data;
extern const dx_screen_t dx_screen_map;
extern const dx_screen_t dx_screen_radio;

/* The table dx_main() runs. The gallery ROM defines its own. */
extern const dx_screen_t *const dx_rom_screens[];
extern const int dx_rom_screen_count;

/* "<file> missing", "<file> line N" and the like for a file that did
 * not load, into buf of `cap` bytes. Returns buf. */
struct dx_load;
const char *dx_file_reason(const struct dx_load *ld, const char *file, char *buf, int cap);

#endif
