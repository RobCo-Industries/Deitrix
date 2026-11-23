/*
 * The held-key rule, one for every tab. A fresh press that changes the
 * level (a screen opens or closes a nested view, or the shell changes
 * the tab) latches its key. The repeats of that key are then swallowed
 * until the next fresh event of any key, so a held LEFT that closed a
 * view does not go on to change the sub-tab or the tab, and a held
 * SELECT that opened one does not act inside it. The shell applies it
 * in dx_app_route(), so no screen has to. Repeats that do not follow a
 * level change still reach the screen: UP and DOWN in lists and text,
 * and value changes.
 */
#ifndef DX_UI_LATCH_H
#define DX_UI_LATCH_H

#include "ui/ui.h"

typedef struct dx_latch {
    int on;  /* 1 while the repeats of key are swallowed */
    int key; /* DX_UI_* of the press that changed the level */
} dx_latch_t;

/* Clears the latch, with no key pressed yet. */
void dx_latch_init(dx_latch_t *l);

/* 1 when the event is a repeat of the latched key. A fresh event of any
 * key, the dial included, clears the latch first. A screen still opens
 * or closes a level on a fresh press only, since a repeat may come with
 * no press before it, as after a tab change by the dial. */
int dx_latch_swallow(dx_latch_t *l, const dx_ui_event_t *ev);

/* Latches the key of a fresh press whose result has DX_UI_LEVEL, or
 * that changed the tab. */
void dx_latch_note(dx_latch_t *l, const dx_ui_event_t *ev, unsigned result, int tab_changed);

#endif
