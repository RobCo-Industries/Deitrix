/*
 * Culvert's screens, through the kernel's drawing calls.
 */
#ifndef CV_DRAW_H
#define CV_DRAW_H

#include <stdint.h>

#include "game.h"

void cv_draw_init(void);
/* Draws what the effect bits of the last step ask for, then presents. */
void cv_draw(const cv_game_t *g, uint32_t now);
/* The whole grid, for the redraw time. */
void cv_draw_grid(const cv_game_t *g);

#endif
