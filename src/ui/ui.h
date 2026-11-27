/*
 * The UI framework's core types: the theme, the layout, input events,
 * the drawing context and the screen interface. Pure code.
 */
#ifndef DX_UI_UI_H
#define DX_UI_UI_H

#include <stddef.h>
#include <stdint.h>

#include "platform/platform.h"

/* ---- Layout: the bands of the 320x240 screen ---------------------------- */

#define DX_HEADER_RULE 21  /* rule under the tab bar */
#define DX_BODY_TOP    22  /* first row of the body */
#define DX_BODY_BOTTOM 214 /* last row of the body */
#define DX_BODY_H      (DX_BODY_BOTTOM - DX_BODY_TOP + 1)
#define DX_FOOTER_RULE 215 /* rule over the key hint */
#define DX_FOOTER_Y    220 /* text row of the key hint */
#define DX_ROW_H       DX_FONT_H
#define DX_MAX_TABS    8
#define DX_TAB_NAME_MAX 5 /* letters in a tab label */

typedef struct dx_rect {
    int x, y, w, h;
} dx_rect_t;

/* ---- Theme -------------------------------------------------------------- */

typedef struct dx_theme {
    uint32_t fg;    /* active items, from the card's scheme */
    uint32_t dim;   /* inactive items: fg at 1/2 per channel */
    uint32_t faint; /* tracks and guides: fg at 1/4 per channel */
    uint32_t bg;    /* black */
} dx_theme_t;

dx_theme_t dx_theme_from_fg(uint32_t fg);

/* ---- Events ------------------------------------------------------------- */

enum {
    DX_UI_UP,
    DX_UI_DOWN,
    DX_UI_LEFT,
    DX_UI_RIGHT,
    DX_UI_SELECT,
    DX_UI_BACK, /* the Back button, new in ABI 1.2: see below */
    DX_UI_DIAL,
};

typedef struct dx_ui_event {
    int type;      /* DX_UI_* */
    int repeat;    /* 1 for a held button's repeat */
    int32_t delta; /* DX_UI_DIAL: signed detents */
} dx_ui_event_t;

/*
 * Navigation:
 * - In a detail view or any nested level, BACK and LEFT both mean "back
 *   one level". The screen handles them, and RIGHT too, so no key
 *   changes the tab there.
 * - At the top level of a tab, LEFT and RIGHT move between sub-tabs.
 *   When the screen ignores them, the shell changes the tab. The shell
 *   ignores BACK: it never changes the tab.
 * - The dial push is SELECT. The dial turn always changes the tab.
 * A screen returns DX_UI_LEVEL with an event that opens or closes a
 * level; the shell then swallows that key's repeats (ui/latch.h).
 * A screen with levels tests dx_ui_is_back(), so the Back button, the
 * QEMU console's Backspace and LEFT all work the same. A Pip-OS older
 * than ABI 1.2 never sends Back: LEFT still works there.
 */

/* 1 for DX_UI_BACK and DX_UI_LEFT, repeats too, else 0. */
int dx_ui_is_back(const dx_ui_event_t *ev);

/*
 * 1 and *out for a press, a repeat or a dial turn. 0 for a release or
 * an unknown event. The dial push is SELECT, button 6 is BACK.
 */
int dx_ui_translate(const dx_event_t *ev, dx_ui_event_t *out);

/* What an event handler returns: DX_UI_IGNORED, or DX_UI_HANDLED,
 * optionally with DX_UI_DIRTY to ask for a redraw. */
#define DX_UI_IGNORED 0u
#define DX_UI_HANDLED 1u
#define DX_UI_DIRTY   2u
/* Set with DX_UI_HANDLED when the event opened or closed a nested view.
 * The shell then swallows the repeats of that key: see ui/latch.h. */
#define DX_UI_LEVEL   4u

/* ---- Tick arithmetic ---------------------------------------------------- */

/* The Pip-OS ABI: ticks_ms "wraps at 2^31 ... Measure the time from a to
 * b as (b - a) & 0x7FFFFFFF. Plain unsigned subtraction is wrong across
 * the wrap". Every comparison, difference or sum of tick values goes
 * through these helpers. */
#define DX_TICKS_MASK 0x7fffffffu
/* The longest interval a screen may schedule: one day, far below 2^30. */
#define DX_TICKS_MAX_MS 86400000u

/* The time from tick a to tick b, in 0 .. 2^31 - 1. */
uint32_t dx_ticks_since(uint32_t a, uint32_t b);
/* The tick `ms` after t, wrapped at 2^31. Never 0, since 0 means "no
 * wake": a sum that lands on 0 becomes 1. `ms` above DX_TICKS_MAX_MS is
 * cut to it. */
uint32_t dx_ticks_add(uint32_t t, uint32_t ms);
/* 1 when `wake` is at or before `now`, less than 2^30 ms in the past. A
 * wake further back reads as a future one: the loop checks often enough
 * that this never happens. */
int dx_ticks_due(uint32_t now, uint32_t wake);

/* ---- Drawing context ---------------------------------------------------- */

typedef struct dx_ctx {
    const dx_gfx_t *gfx;
    dx_theme_t theme;
    uint32_t now_ms;
} dx_ctx_t;

/* ---- Screens ------------------------------------------------------------ */

typedef struct dx_screen {
    const char *name;                       /* tab label, at most 5 letters */
    void (*enter)(void *st);                /* the tab becomes active */
    void (*leave)(void *st);
    unsigned (*event)(void *st, const dx_ui_event_t *ev);
    /* Called when its wake time passes. Returns the next wake time in
     * ticks, or 0 for none. May set DX_UI_DIRTY in *flags. Build the wake
     * time with dx_ticks_add(now_ms, ms), ms at most DX_TICKS_MAX_MS: the
     * tick counter wraps at 2^31, and the helper never returns 0. */
    uint32_t (*tick)(void *st, uint32_t now_ms, unsigned *flags);
    void (*draw)(void *st, const dx_ctx_t *ctx, dx_rect_t area); /* the body only */
    const char *(*hint)(void *st); /* footer text, or NULL for the default */
    void *st;
} dx_screen_t;

/* ---- Sounds ------------------------------------------------------------- */

enum { DX_SND_NONE, DX_SND_MOVE, DX_SND_SELECT, DX_SND_BACK, DX_SND_DENY };

/* 0 turns every UI sound off. */
#define DX_UI_SOUND 1

/* Plays a short tone for a DX_SND_*, unless DX_UI_SOUND is 0, the
 * owner turned UI sounds off, or the radio holds the voice. */
void dx_ui_sound(int snd);

/* The owner's setting: 0 turns UI sounds off. On at start. */
void dx_ui_sound_enable(int on);

/* 1 when UI sounds are on: DX_UI_SOUND and the owner's setting. */
int dx_ui_sound_enabled(void);

/*
 * Pip-OS has one voice, and a tone replaces the sound that plays. So
 * while the radio plays, it blocks UI sounds: a tab change does not cut
 * the radio. 1 blocks, 0 lets them play again.
 */
void dx_ui_sound_block(int blocked);

/*
 * The sound for an event's outcome: MOVE for a tab change, SELECT or
 * BACK when the screen handled one, MOVE when it handled another key
 * and asked for a redraw, DENY when it ignored UP, DOWN, SELECT or BACK
 * on a fresh press, else none.
 */
int dx_ui_sound_for(const dx_ui_event_t *ev, unsigned result, int tab_changed);

/* ---- Text helpers ------------------------------------------------------- */

int dx_strlen(const char *s);

/* The width in pixels of text at a scale. */
int dx_text_width(const char *text, int scale);

/*
 * Draws at most `cols` characters of s at scale 1. When s is longer,
 * draws cols - 1 characters and the cut marker '~'. Returns the
 * characters drawn.
 */
int dx_text_cut(const dx_ctx_t *ctx, int x, int y, const char *s, int cols, uint32_t fg,
                uint32_t bg);

/* Writes v in decimal, NUL-terminated, returns the length. buf holds 12. */
int dx_itoa(int32_t v, char *buf);

/* Draws a vertical scroll bar in the rect: a faint track and a thumb
 * for `visible` of `total` rows from `first`. Nothing when all fit. */
void dx_scrollbar(const dx_ctx_t *ctx, dx_rect_t r, int first, int visible, int total);

#define DX_SCROLLBAR_W 4

#endif
