/*
 * Pure helpers of the MAP tab: map lines to pixels through a fit, the
 * place cycling, the marker squares and the position text.
 */
#ifndef DX_MODEL_MAP_VIEW_H
#define DX_MODEL_MAP_VIEW_H

#include <stdint.h>

#include "model/map.h"

/* A place: a filled square of DX_MAP_MARK px. The selected place: a
 * reticle, a ring of DX_MAP_MARK_SEL px around a filled square of
 * DX_MAP_DOT px, with a tick of DX_MAP_TICK px out of each side. */
#define DX_MAP_MARK     5
#define DX_MAP_MARK_SEL 9
#define DX_MAP_DOT      3
#define DX_MAP_TICK     3
/* How far the reticle reaches from its centre pixel. */
#define DX_MAP_REACH    (DX_MAP_MARK_SEL / 2 + DX_MAP_TICK)
/* The space between a pane's edge and the fitted box: the reticle's
 * reach and one pixel more, so no marker reaches the edge. */
#define DX_MAP_MARGIN   (DX_MAP_REACH + 1)

/* "x, y" of a map point: at most 8 characters and the NUL. */
#define DX_MAP_POS_SIZE 12

typedef struct dx_map_square {
    int x, y, size;
} dx_map_square_t;

/* The fit of map m into the pane x, y, w, h, DX_MAP_MARGIN inside its
 * edges. Every point of the box lands inside the inset rectangle. */
dx_map_fit_t dx_map_pane_fit(const dx_map_t *m, int x, int y, int w, int h);

/* Writes line l as pixels, x then y, into xy (2 * DX_POINTS_MAX
 * values). Returns the point count. Rounds as dx_map_to_px. */
int dx_map_line_px(const dx_map_fit_t *f, const dx_polyline_t *l, int16_t *xy);

/*
 * The place after `sel` in the direction of `step` (+1 or -1) among
 * `count`, wrapping at both ends. -1 when count is 0. A sel outside
 * 0 .. count - 1 starts again at 0.
 */
int dx_map_cycle(int sel, int count, int step);

/* The square of `size` px centred on pixel px, py: size is odd. */
dx_map_square_t dx_map_square(int px, int py, int size);

/* "x, y" as the file gives them, into buf of DX_MAP_POS_SIZE bytes. */
const char *dx_map_pos_text(dx_point_t p, char *buf);

#endif
