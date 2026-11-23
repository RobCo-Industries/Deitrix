/*
 * A stroke font for large text: digits, A to Z, space and ": - . / %".
 * Each glyph is a few polylines on a grid 4 units wide and 6 high,
 * drawn with lines at any whole unit size. Unit 4 gives a 16x24 px cell.
 */
#ifndef DX_UI_VFONT_H
#define DX_UI_VFONT_H

#include "ui/ui.h"

#define DX_VFONT_W 4 /* grid units: x 0 to 4 */
#define DX_VFONT_H 6 /* y 0 to 6 */

/*
 * The strokes of c: polylines separated by '|', each point two digits,
 * x then y. Lowercase letters use the capitals. NULL for a character
 * the font lacks; dx_vfont_draw draws a crossed box for it.
 */
const char *dx_vfont_glyph(char c);

/* The advance of c in units: 3 for ':' and '.', else 6. */
int dx_vfont_advance(char c);

/* The width of s in pixels at `unit`: the sum of its advances. */
int dx_vfont_width(const char *s, int unit);

/* Draws s with its top left at x, y. Returns the width. */
int dx_vfont_draw(const dx_ctx_t *ctx, int x, int y, const char *s, int unit, uint32_t color);

#endif
