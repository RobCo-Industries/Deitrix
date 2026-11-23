/*
 * Deitrix: the entry and the app shell. Pip-OS starts the ROM in
 * platform.c, which calls dx_main().
 */
#include "app.h"

#include "model/clock.h"
#include "model/nav.h"
#include "platform/platform.h"
#include "screens/screens.h"
#include "ui/frame.h"

#define LOG_MAX 32

static const dx_screen_t *active(const dx_app_t *app)
{
    return app->screens[app->tab];
}

/* Starts the screen's tick at once when it has one. */
static void enter(dx_app_t *app)
{
    const dx_screen_t *s = active(app);

    if (s->enter != NULL) {
        s->enter(s->st);
    }
    app->wake_on = s->tick != NULL;
    app->wake = dx_plat_ticks();
}

int dx_app_init(dx_app_t *app, const dx_screen_t *const *screens, int count)
{
    int i;

    app->screens = screens;
    app->count = 0;
    if (count < 1 || count > DX_MAX_TABS) {
        dx_plat_log("deitrix: refused: the tab table needs 1 to 8 screens\n");
        return -1;
    }
    for (i = 0; i < count; i++) {
        app->names[i] = screens[i]->name;
    }
    if (dx_frame_tab_cut(app->names, count) == 0) {
        dx_plat_log("deitrix: refused: tab names need 1 to 5 letters\n");
        return -1;
    }
    app->count = count;
    app->ctx.gfx = dx_plat_gfx();
    app->ctx.theme = dx_theme_from_fg(dx_plat_fg());
    app->ctx.now_ms = 0;
    app->tab = 0;
    app->drawn_tab = -1;
    app->drawn_min = DX_CLOCK_NONE;
    app->dirty = 0;
    enter(app);
    return 0;
}

static void log_tab(const char *name)
{
    static const char prefix[] = "deitrix: tab=";
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
    const dx_screen_t *s = active(app);
    const dx_gfx_t *g = app->ctx.gfx;
    const char *hint = s->hint != NULL ? s->hint(s->st) : NULL;
    dx_rect_t body = dx_frame_body();
    char clock[DX_CLOCK_TEXT_SIZE];
    int first = app->drawn_tab < 0;
    int tab_changed = app->tab != app->drawn_tab;

    dx_clock_text(minute, clock);
    g->clear(app->ctx.theme.bg);
    dx_frame_header(&app->ctx, app->names, app->count, app->tab, clock);
    if (s->draw != NULL) {
        g->clip(body.x, body.y, body.w, body.h);
        s->draw(s->st, &app->ctx, body);
        g->clip(0, 0, 0, 0);
    }
    dx_frame_footer(&app->ctx, hint != NULL ? hint : DX_FRAME_HINT);
    g->present();
    app->drawn_tab = app->tab;
    app->drawn_min = minute;
    app->dirty = 0;
    /* Logged after present, so a test that sees the line sees the frame. */
    if (first) {
        dx_plat_log("deitrix: ready\n");
    } else if (tab_changed) {
        log_tab(s->name);
    }
}

/* 1 when the tab changed. */
static int change_tab(dx_app_t *app, int32_t delta)
{
    int next = dx_nav_move(app->tab, delta, app->count);
    const dx_screen_t *s = active(app);

    if (next == app->tab) {
        return 0;
    }
    if (s->leave != NULL) {
        s->leave(s->st);
    }
    app->tab = next;
    enter(app);
    return 1;
}

void dx_app_route(dx_app_t *app, const dx_ui_event_t *ev)
{
    const dx_screen_t *s = active(app);
    unsigned result = DX_UI_IGNORED;
    int changed = 0;

    if (ev->type == DX_UI_DIAL) {
        changed = change_tab(app, ev->delta);
    } else {
        if (s->event != NULL) {
            result = s->event(s->st, ev);
        }
        if (result & DX_UI_DIRTY) {
            app->dirty = 1;
        }
        if (result & DX_UI_HANDLED) {
            /* The screen may want its tick again after an event. */
            app->wake_on = s->tick != NULL;
            app->wake = dx_plat_ticks();
        } else if (ev->type == DX_UI_LEFT) {
            changed = change_tab(app, -1);
        } else if (ev->type == DX_UI_RIGHT) {
            changed = change_tab(app, 1);
        }
    }
    dx_ui_sound(dx_ui_sound_for(ev, result, changed));
}

/* 1 when tick time `t` is at or after `when`, across the wrap. */
static int due(uint32_t t, uint32_t when)
{
    return (int32_t)(t - when) >= 0;
}

void dx_app_step(dx_app_t *app)
{
    const dx_screen_t *s = active(app);
    uint32_t now = 0;
    int valid = dx_plat_time(&now);
    int32_t minute = dx_clock_minute(valid, now);
    uint32_t ticks = dx_plat_ticks();
    uint32_t timeout = valid ? dx_clock_wait_ms(now) : DX_WAIT_FOREVER;
    dx_event_t ev;
    dx_ui_event_t uev;

    app->ctx.now_ms = ticks;
    if (app->wake_on && due(ticks, app->wake)) {
        unsigned flags = 0;
        uint32_t next = s->tick(s->st, ticks, &flags);

        app->wake_on = next != 0;
        app->wake = next;
        if (flags & DX_UI_DIRTY) {
            app->dirty = 1;
        }
    }
    if (app->dirty || app->tab != app->drawn_tab || minute != app->drawn_min) {
        draw(app, minute);
    }
    if (app->wake_on) {
        uint32_t until = due(ticks, app->wake) ? 0 : app->wake - ticks;

        if (until < timeout) {
            timeout = until;
        }
    }
    if (dx_plat_wait(&ev, timeout) && dx_ui_translate(&ev, &uev)) {
        dx_app_route(app, &uev);
    }
}

void dx_main(void)
{
    static dx_app_t app;

    if (dx_app_init(&app, dx_rom_screens, dx_rom_screen_count) != 0) {
        /* A table the header cannot draw: stop here, the log says why. */
        for (;;) {
            dx_event_t ev;

            dx_plat_wait(&ev, DX_WAIT_FOREVER);
        }
    }
    for (;;) {
        dx_app_step(&app);
    }
}
