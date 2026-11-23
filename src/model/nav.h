/*
 * The tab list and the navigation state machine. Pure code: no system
 * calls, so the host tests build it as it is.
 */
#ifndef DX_MODEL_NAV_H
#define DX_MODEL_NAV_H

#include <stdint.h>

#include "platform/platform.h"

#define DX_TAB_COUNT 5

enum { DX_TAB_STAT, DX_TAB_INV, DX_TAB_DATA, DX_TAB_MAP, DX_TAB_RADIO };

/* The tab's name, or "" for an index out of range. */
const char *dx_tab_name(int tab);

/* The tab `delta` steps after `tab`, wrapping in both directions. */
int dx_nav_move(int tab, int32_t delta);

/*
 * The tab steps an input event asks for: the detents of a dial event,
 * -1 for a LEFT press or repeat, +1 for RIGHT, 0 for anything else.
 */
int32_t dx_nav_delta(const dx_event_t *ev);

#endif
