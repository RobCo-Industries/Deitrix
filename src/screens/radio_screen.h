/* The RADIO tab's layout and state, for the host tests. */
#ifndef DX_SCREENS_RADIO_SCREEN_H
#define DX_SCREENS_RADIO_SCREEN_H

#include "ui/grid.h"

/* The layout of the body, from its top. */
#define DX_RADIO_LIST_DY   DX_GRID_TOP /* the stations, one row each */
#define DX_RADIO_LIST_ROWS 6   /* DX_STATIONS_MAX */
#define DX_RADIO_RULE1_DY  106 /* a faint rule over the now playing block */
#define DX_RADIO_NOW_DY    112 /* its two rows */
#define DX_RADIO_RULE2_DY  150 /* a faint rule over the settings */
#define DX_RADIO_SET_DY    156 /* Volume, then UI sounds */

/* LEFT and RIGHT on the Volume row move to the next multiple of this. */
#define DX_RADIO_VOLUME_STEP 10

/* Forgets the selection: the next frame or key takes it from the
 * settings' last station again. The ROM never calls it. */
void dx_radio_screen_reset(void);

#endif
