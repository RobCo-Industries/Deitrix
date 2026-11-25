/*
 * The frame around every screen: the tab bar with the clock at the top
 * and the key hint at the bottom.
 */
#ifndef DX_UI_FRAME_H
#define DX_UI_FRAME_H

#include "ui/ui.h"

/*
 * The footer hints follow one style, checked by tests/host/test_hints.c
 * and listed in docs/ARCHITECTURE.md: "KEY: action" groups two spaces
 * apart, the keys UP/DOWN, LEFT/RIGHT, SELECT, BACK and DIAL, movement
 * first, then SELECT, then BACK, at most DX_HINT_MAX characters. A
 * screen's hint never names the tab change: the header shows the tabs.
 * DX_FRAME_HINT is the shell's own, for a state with no key of its own.
 */
#define DX_HINT_MAX   38
#define DX_FRAME_HINT "DIAL: change tab"
/* A top level whose only keys move between sub-tabs. */
#define DX_HINT_PAGE  "LEFT/RIGHT: page"

/* The body area between the header and the footer rules. */
dx_rect_t dx_frame_body(void);

/* The clock's x, and the right limit of the tab bar: 8 px before it. */
#define DX_CLOCK_X     (DX_SCREEN_W - 4 - 5 * DX_FONT_W)
#define DX_TAB_RIGHT   (DX_CLOCK_X - 8)
#define DX_TAB_MIN_PAD 2 /* the least space on each side of a label */
#define DX_TAB_CUT_MIN 3 /* labels are never cut shorter */

/*
 * How many letters of each tab label the header draws: DX_TAB_NAME_MAX
 * when the labels fit as they are, else 4, then 3. Labels fit when they
 * end before DX_TAB_RIGHT with DX_TAB_MIN_PAD px on each side.
 * 0 when the table cannot be drawn: no tabs, more than DX_MAX_TABS, an
 * empty or NULL name, a name longer than DX_TAB_NAME_MAX, or labels that
 * do not fit even at 3 letters. With the limits in ui.h the last case
 * cannot happen: 8 labels of 3 letters take 224 px of the 264.
 */
int dx_frame_tab_cut(const char *const *names, int count);

/* The space on each side of a tab label: 8 px, or less, down to
 * DX_TAB_MIN_PAD, to fit. 0 when dx_frame_tab_cut is 0. */
int dx_frame_tab_pad(const char *const *names, int count);

/* The x of tab label `tab`. */
int dx_frame_tab_x(const char *const *names, int count, int tab);

/* Draws the tab bar, the clock ("HH:MM" or "--:--") and the rule. A
 * table dx_frame_tab_cut refuses draws no tabs. */
void dx_frame_header(const dx_ctx_t *ctx, const char *const *names, int count, int active,
                     const char *clock);

/* The unsaved mark: a "!" in fg in the gap left of the clock. */
#define DX_MARK_X (DX_CLOCK_X - DX_FONT_W)
void dx_frame_mark(const dx_ctx_t *ctx);

/* Draws the rule and the hint, centred. */
void dx_frame_footer(const dx_ctx_t *ctx, const char *hint);

/* Draws the rule and a notice in place of the hint: centred, in fg
 * between two small marks, so it reads apart from a dim hint. */
void dx_frame_notice(const dx_ctx_t *ctx, const char *text);

/* Draws text centred on the screen width at y. */
void dx_text_centered(const dx_ctx_t *ctx, int y, const char *text, uint32_t color, int scale);

#endif
