/* The RADIO tab's layout and state, for the host tests. */
#ifndef DX_SCREENS_RADIO_SCREEN_H
#define DX_SCREENS_RADIO_SCREEN_H

#include "ui/grid.h"
#include "ui/ui.h"

/* The layout of the body, from its top. */
#define DX_RADIO_LIST_DY   DX_GRID_TOP /* the stations, one row each */
#define DX_RADIO_LIST_ROWS 6   /* DX_STATIONS_MAX */
#define DX_RADIO_RULE1_DY  106 /* a faint rule over the now playing block */
#define DX_RADIO_NOW_DY    112 /* its two rows */
#define DX_RADIO_RULE2_DY  150 /* a faint rule over the settings */
#define DX_RADIO_SET_DY    156 /* Volume, then UI sounds */

/* LEFT and RIGHT on the Volume row move to the next multiple of this. */
#define DX_RADIO_VOLUME_STEP 10
/* LEFT and RIGHT in the levels view move to the next multiple of this. */
#define DX_RADIO_LEVEL_STEP 5

/* The levels view, from the top of the body. */
#define DX_RADIO_LEVELS_TITLE_DY DX_GRID_BAND_TEXT_DY   /* "SOUND LEVELS" */
#define DX_RADIO_LEVELS_RULE_DY  (DX_GRID_BAND_H - 1)   /* a faint rule under it */
#define DX_RADIO_LEVELS_ROWS_DY  DX_GRID_BAND_TOP       /* four gauges, one row each */
#define DX_RADIO_LEVELS_ROWS     4
#define DX_RADIO_LEVELS_NOTE_DY  98 /* the selected row's line, 8 px under the rows */

/* Forgets the selection: the next frame or key takes it from the
 * settings' last station again. The ROM never calls it. */
void dx_radio_screen_reset(void);

/*
 * A gauge row of the tab: the label, the bar from x 108 and three
 * digits at the right, inverse when selected. The Volume row and the
 * levels view draw with it.
 */
void dx_radio_gauge_row(const dx_ctx_t *ctx, dx_rect_t area, int y, const char *label, int value,
                        int selected);

/*
 * The levels view: a nested level of the tab, opened by SELECT on the
 * Volume row, with one gauge for each kind of sound. It exists from
 * ABI 1.6 only.
 */
/* 1 when the kernel has the level calls, else 0. */
int dx_radio_levels_there(void);
/* 1 while the view is open. */
int dx_radio_levels_is_open(void);
/* Reads the four levels and opens the view on its first row. Returns
 * DX_UI_IGNORED and stays closed when the view is not there or a read
 * fails. */
unsigned dx_radio_levels_open(void);
/* Closes the view. Logs nothing. */
void dx_radio_levels_close(void);
/* One key of the open view. Logs one line for each key. */
unsigned dx_radio_levels_event(const dx_ui_event_t *ev);
/* Draws the open view into the body. */
void dx_radio_levels_draw(const dx_ctx_t *ctx, dx_rect_t area);

#endif
