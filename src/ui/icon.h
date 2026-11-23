/* 1-bit icons. tools/pbm2c.py turns the PBM files of assets/ into dx_icon_t data. */
#ifndef DX_UI_ICON_H
#define DX_UI_ICON_H

#include "ui/ui.h"

typedef struct dx_icon {
    const char *name;
    int w, h;
    int stride; /* bytes per row */
    const uint8_t *bits; /* rows packed most significant bit first */
} dx_icon_t;

/* The icons from assets/, in name order, generated at build time. */
extern const dx_icon_t dx_icons[];
extern const int dx_icon_count;

/* The icon with this name, or NULL. */
const dx_icon_t *dx_icon_find(const char *name);

/* Draws the icon centred in area, set bits in color. */
void dx_icon_draw(const dx_icon_t *icon, const dx_ctx_t *ctx, dx_rect_t area, uint32_t color);

#endif
