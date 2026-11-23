/*
 * Deitrix: the entry and the main loop. Pip-OS starts the ROM in
 * platform.c, which calls dx_main().
 */
#include "app.h"

#include "model/clock.h"
#include "model/nav.h"
#include "platform/platform.h"
#include "screens/placeholder.h"

#define LOG_MAX 32

void dx_app_init(dx_app_t *app)
{
    app->theme = dx_theme_from_fg(dx_plat_fg());
    app->tab = DX_TAB_STAT;
    app->drawn_tab = -1;
    app->drawn_min = DX_CLOCK_NONE;
}

static void log_tab(int tab)
{
    static const char prefix[] = "deitrix: tab=";
    const char *name = dx_tab_name(tab);
    char line[LOG_MAX];
    int n = 0;
    int i;

    for (i = 0; prefix[i] != '\0'; i++) {
        line[n++] = prefix[i];
    }
    for (i = 0; name[i] != '\0' && n < LOG_MAX - 2; i++) {
        line[n++] = name[i];
    }
    line[n++] = '\n';
    line[n] = '\0';
    dx_plat_log(line);
}

static void draw(dx_app_t *app, int32_t minute)
{
    char clock[DX_CLOCK_TEXT_SIZE];
    int first = app->drawn_tab < 0;
    int tab_changed = app->tab != app->drawn_tab;

    dx_clock_text(minute, clock);
    dx_plat_clear(0);
    dx_frame_draw(&app->theme, app->tab, clock);
    dx_placeholder_draw(&app->theme, app->tab);
    dx_plat_present();
    app->drawn_tab = app->tab;
    app->drawn_min = minute;
    /* Logged after present, so a test that sees the line sees the frame. */
    if (first) {
        dx_plat_log("deitrix: ready\n");
    } else if (tab_changed) {
        log_tab(app->tab);
    }
}

void dx_app_step(dx_app_t *app)
{
    uint32_t now = 0;
    int valid = dx_plat_time(&now);
    int32_t minute = dx_clock_minute(valid, now);
    dx_event_t ev;

    if (app->tab != app->drawn_tab || minute != app->drawn_min) {
        draw(app, minute);
    }
    if (dx_plat_wait(&ev, valid ? dx_clock_wait_ms(now) : DX_WAIT_FOREVER)) {
        int32_t delta = dx_nav_delta(&ev);

        if (delta != 0) {
            app->tab = dx_nav_move(app->tab, delta);
        }
    }
}

void dx_main(void)
{
    dx_app_t app;

    dx_app_init(&app);
    for (;;) {
        dx_app_step(&app);
    }
}
