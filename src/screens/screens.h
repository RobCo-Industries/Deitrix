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

/* The body of a screen not built yet: its name in large text and a note. */
void dx_placeholder_draw(const dx_ctx_t *ctx, dx_rect_t area, const char *name);

#endif
