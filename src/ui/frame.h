/*
 * The frame around every screen: the tab bar with the clock at the top
 * and the key hint at the bottom. Draws through the platform layer only.
 */
#ifndef DX_UI_FRAME_H
#define DX_UI_FRAME_H

#include <stdint.h>

/* The body area between the header and the footer rules. */
#define DX_BODY_TOP    22
#define DX_BODY_BOTTOM 214

typedef struct dx_theme {
    uint32_t fg;  /* active items, from the card's scheme */
    uint32_t dim; /* inactive items */
} dx_theme_t;

/* The theme for a scheme colour: dim is fg at half brightness. */
dx_theme_t dx_theme_from_fg(uint32_t fg);

/* The width in pixels of text at a scale. */
int32_t dx_text_width(const char *text, uint32_t scale);

/* Draws text centred on the screen width at y. */
void dx_text_centered(int32_t y, const char *text, uint32_t scale);

/* The x of each tab label in the header, for tab 0 to DX_TAB_COUNT - 1. */
int32_t dx_frame_tab_x(int tab);

/* Draws the header and the footer. clock is the "HH:MM" or "--:--" text. */
void dx_frame_draw(const dx_theme_t *theme, int active_tab, const char *clock);

#endif
