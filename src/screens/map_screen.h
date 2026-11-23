/* The MAP tab's layout, for the screen and its host tests. */
#ifndef DX_SCREENS_MAP_SCREEN_H
#define DX_SCREENS_MAP_SCREEN_H

#include "ui/grid.h"
#include "ui/ui.h"

/* Offsets in the area under the sub-tab row. The map pane takes the
 * top, a faint rule follows, then the row of the selected place. */

/* The detail view: the title row, the key rows, then the locator map. */
#define DX_MAP_KV_DY     DX_GRID_GAP
#define DX_MAP_KV_ROWS   4
#define DX_MAP_LOC_DY    (DX_MAP_KV_DY + DX_MAP_KV_ROWS * DX_ROW_H + 6)
#define DX_MAP_LOC_W     160

/* Forgets the tab's state: the sub-tab, the selections, the detail. For
 * the host tests; the ROM never calls it. */
void dx_map_reset(void);

#endif
