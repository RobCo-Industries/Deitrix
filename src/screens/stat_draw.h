/*
 * Drawing helpers of the STAT tab: the owner's name and the gauge group,
 * from plain values.
 */
#ifndef DX_SCREENS_STAT_DRAW_H
#define DX_SCREENS_STAT_DRAW_H

#include "ui/ui.h"

/* The largest stroke-font unit the name is drawn at. */
#define DX_STAT_NAME_UNIT_MAX 4
/* Below this unit the name uses the normal font. */
#define DX_STAT_NAME_UNIT_MIN 3

/*
 * The stroke-font unit for `name` in `width` px: DX_STAT_NAME_UNIT_MAX
 * down to DX_STAT_NAME_UNIT_MIN, the largest that fits. 0 when none
 * fits or the font lacks one of its characters: then the name uses the
 * normal font. Pure.
 */
int dx_stat_name_unit(const char *name, int width);

/* Draws the name centred in a band of `h` px from y, across area. */
void dx_stat_draw_name(const dx_ctx_t *ctx, dx_rect_t area, int y, int h, const char *name);

/*
 * Draws n gauges, one per `pitch` px from area's top, with one shared
 * label column, the longest label, and one shared value column.
 */
void dx_stat_draw_gauges(const dx_ctx_t *ctx, dx_rect_t area, int pitch,
                         const char *const *labels, const int32_t *values,
                         const int32_t *maxes, int n);

/* Forgets the STAT tab's state: the sub-tab, the list, what ABOUT
 * showed. For the host tests; the ROM never calls it. */
void dx_stat_reset(void);

#endif
