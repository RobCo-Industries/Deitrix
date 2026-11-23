/*
 * The frame around every screen: the tab bar with the clock at the top
 * and the key hint at the bottom.
 */
#ifndef DX_UI_FRAME_H
#define DX_UI_FRAME_H

#include "ui/ui.h"

/* The footer text when a screen has no hint of its own. */
#define DX_FRAME_HINT "DIAL or LEFT/RIGHT: change tab"

/* The body area between the header and the footer rules. */
dx_rect_t dx_frame_body(void);

/*
 * The space on each side of a tab label: 8 px, or less when `count`
 * labels would reach the clock.
 */
int dx_frame_tab_pad(const char *const *names, int count);

/* The x of tab label `tab`. */
int dx_frame_tab_x(const char *const *names, int count, int tab);

/* Draws the tab bar, the clock ("HH:MM" or "--:--") and the rule. */
void dx_frame_header(const dx_ctx_t *ctx, const char *const *names, int count, int active,
                     const char *clock);

/* Draws the rule and the hint, centred. */
void dx_frame_footer(const dx_ctx_t *ctx, const char *hint);

/* Draws text centred on the screen width at y. */
void dx_text_centered(const dx_ctx_t *ctx, int y, const char *text, uint32_t color, int scale);

#endif
