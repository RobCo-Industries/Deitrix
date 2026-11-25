/*
 * The layout grid of the body, one for every tab. Offsets are from the
 * top of the body, DX_BODY_TOP, unless they say otherwise.
 *
 * - A band at the top holds the sub-tab row or a detail's title row,
 *   DX_GRID_BAND_H high with a faint rule on its last line. Its text
 *   sits at DX_GRID_BAND_TEXT_DY.
 * - Content starts DX_GRID_GAP under the band, at DX_GRID_BAND_TOP, or
 *   DX_GRID_GAP under the header rule when there is no band.
 * - A bottom status or summary row sits at DX_GRID_STATUS_DY, 2 px over
 *   the body's end, with a faint rule at DX_GRID_STATUS_RULE_DY.
 * - An empty-state message is centred in the area under the band, or in
 *   the body when there is no band.
 */
#ifndef DX_UI_GRID_H
#define DX_UI_GRID_H

#include "ui/subtabs.h"
#include "ui/ui.h"

#define DX_GRID_GAP            6
#define DX_GRID_BAND_H         DX_SUBTABS_H
#define DX_GRID_BAND_TEXT_DY   1
#define DX_GRID_TOP            DX_GRID_GAP
#define DX_GRID_BAND_TOP       (DX_GRID_BAND_H + DX_GRID_GAP)
#define DX_GRID_STATUS_DY      (DX_BODY_H - DX_ROW_H - 2)
#define DX_GRID_STATUS_RULE_DY (DX_GRID_STATUS_DY - 4)

#endif
