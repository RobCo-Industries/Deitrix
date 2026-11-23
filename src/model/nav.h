/*
 * Tab navigation: the move with wrap-around. Pure code: no system
 * calls, so the host tests build it as it is.
 */
#ifndef DX_MODEL_NAV_H
#define DX_MODEL_NAV_H

#include <stdint.h>

/* The tab `delta` steps after `tab` among `count`, wrapping both ways. */
int dx_nav_move(int tab, int32_t delta, int count);

#endif
