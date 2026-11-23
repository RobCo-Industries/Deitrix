/*
 * The main loop, split into steps so the host tests can run it one
 * iteration at a time against the fake platform.
 */
#ifndef DX_APP_H
#define DX_APP_H

#include <stdint.h>

#include "ui/frame.h"

typedef struct dx_app {
    dx_theme_t theme;
    int tab;            /* the active tab */
    int drawn_tab;      /* the tab on screen, -1 before the first frame */
    int32_t drawn_min;  /* the minute on screen */
} dx_app_t;

void dx_app_init(dx_app_t *app);

/*
 * One iteration: redraws and presents when the tab or the minute
 * changed, then waits for one event or the next minute change.
 */
void dx_app_step(dx_app_t *app);

/* Runs dx_app_step forever. */
void dx_main(void) __attribute__((noreturn));

#endif
