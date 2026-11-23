/*
 * The app shell: the main loop over a table of screens, split into
 * steps so the host tests can run it one iteration at a time against
 * the fake platform.
 */
#ifndef DX_APP_H
#define DX_APP_H

#include <stdint.h>

#include "ui/ui.h"

/*
 * A background service: work that goes on whatever tab is shown, such
 * as the radio and the autosave. tick runs when its wake time passes,
 * and after every routed event so it sees changes at once. It returns
 * its next wake time, built with dx_ticks_add(), or 0 for none.
 */
typedef struct dx_service {
    const char *name;
    void (*init)(uint32_t now_ms);
    uint32_t (*tick)(uint32_t now_ms);
    void (*stop)(void);
} dx_service_t;

#define DX_SERVICES_MAX 4

typedef struct dx_app {
    const dx_screen_t *const *screens;
    int count;
    const char *names[DX_MAX_TABS];
    dx_ctx_t ctx;
    int tab;           /* the active tab */
    int drawn_tab;     /* the tab on screen, -1 before the first frame */
    int32_t drawn_min; /* the minute on screen */
    int dirty;         /* the active screen asked for a redraw */
    int wake_on;       /* the active screen has a wake time */
    uint32_t wake;     /* its wake time, in ticks */
    const dx_service_t *const *services;
    int service_count;
    int svc_on[DX_SERVICES_MAX];       /* the service has a wake time */
    uint32_t svc_wake[DX_SERVICES_MAX];
    void (*tab_changed)(int tab);      /* optional: told after each tab change */
} dx_app_t;

/*
 * Checks the table and enters the first screen. Returns 0, or -1 when
 * app or screens is NULL, an entry is NULL, or the header cannot draw
 * the tab labels: no screens, more than DX_MAX_TABS, or a name that is
 * NULL, empty or longer than DX_TAB_NAME_MAX. Nothing is read from an
 * entry before every entry is checked.
 * It logs a "deitrix: refused: ..." line then, and dx_main stops.
 */
int dx_app_init(dx_app_t *app, const dx_screen_t *const *screens, int count);

/*
 * Starts the services of the table, at most DX_SERVICES_MAX: calls each
 * init, then ticks each on the next step. Returns 0, or -1 for a bad
 * table. Call it after dx_app_init.
 */
int dx_app_start_services(dx_app_t *app, const dx_service_t *const *services, int count);

/* Stops every service, in reverse order. */
void dx_app_stop_services(dx_app_t *app);

/* Makes `tab` the active tab before the first frame, as the settings
 * ask. Out of range: nothing. */
void dx_app_set_tab(dx_app_t *app, int tab);

/*
 * One iteration: ticks the services and the screen whose wake times
 * passed, redraws and presents when anything changed, then waits for
 * one event, the next minute change or the earliest wake time, and
 * routes the event.
 */
void dx_app_step(dx_app_t *app);

/* Routes one event: the dial changes the tab, every other event goes to
 * the screen first, and LEFT or RIGHT it ignores change the tab. */
void dx_app_route(dx_app_t *app, const dx_ui_event_t *ev);

/* Runs dx_app_step forever over dx_rom_screens. */
void dx_main(void) __attribute__((noreturn));

#endif
