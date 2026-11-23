/*
 * Deitrix: the entry and the app shell. Pip-OS starts the ROM in
 * platform.c, which calls dx_main().
 */
#include "app.h"

#include "content.h"
#include "model/clock.h"
#include "model/nav.h"
#include "platform/platform.h"
#include "screens/screens.h"
#include "services/services.h"
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

    if (app == NULL) {
        dx_plat_log("deitrix: refused: no app\n");
        return -1;
    }
    app->screens = NULL;
    app->count = 0;
    if (screens == NULL || count < 1 || count > DX_MAX_TABS) {
        dx_plat_log("deitrix: refused: the tab table needs 1 to 8 screens\n");
        return -1;
    }
    /* Every entry is checked before any is read. */
    for (i = 0; i < count; i++) {
        if (screens[i] == NULL) {
            dx_plat_log("deitrix: refused: the tab table has an empty entry\n");
            return -1;
        }
    }
    for (i = 0; i < count; i++) {
        app->names[i] = screens[i]->name;
    }
    if (dx_frame_tab_cut(app->names, count) == 0) {
        dx_plat_log("deitrix: refused: tab names need 1 to 5 letters\n");
        return -1;
    }
    app->screens = screens;
    app->count = count;
    app->ctx.gfx = dx_plat_gfx();
    app->ctx.theme = dx_theme_from_fg(dx_plat_fg());
    app->ctx.now_ms = 0;
    app->tab = 0;
    app->drawn_tab = -1;
    app->drawn_min = DX_CLOCK_NONE;
    app->dirty = 0;
    app->services = NULL;
    app->service_count = 0;
    app->tab_changed = NULL;
    dx_latch_init(&app->latch);
    enter(app);
    return 0;
}

int dx_app_start_services(dx_app_t *app, const dx_service_t *const *services, int count)
{
    uint32_t now = dx_plat_ticks();
    int i;

    if (count < 0 || count > DX_SERVICES_MAX || (count > 0 && services == NULL)) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        if (services[i] == NULL || services[i]->tick == NULL) {
            return -1;
        }
    }
    app->services = services;
    app->service_count = count;
    for (i = 0; i < count; i++) {
        if (services[i]->init != NULL) {
            services[i]->init(now);
        }
        app->svc_on[i] = 1;
        app->svc_wake[i] = now;
    }
    return 0;
}

void dx_app_stop_services(dx_app_t *app)
{
    int i;

    for (i = app->service_count - 1; i >= 0; i--) {
        if (app->services[i]->stop != NULL) {
            app->services[i]->stop();
        }
        app->svc_on[i] = 0;
    }
}

/* Every service runs on the next step: an event may have changed what
 * it watches. */
static void wake_services(dx_app_t *app)
{
    uint32_t now = dx_plat_ticks();
    int i;

    for (i = 0; i < app->service_count; i++) {
        app->svc_on[i] = 1;
        app->svc_wake[i] = now;
    }
}

static void tick_services(dx_app_t *app, uint32_t ticks)
{
    int i;

    for (i = 0; i < app->service_count; i++) {
        if (app->svc_on[i] && dx_ticks_due(ticks, app->svc_wake[i])) {
            unsigned flags = 0;
            uint32_t next = app->services[i]->tick(ticks, &flags);

            app->svc_on[i] = next != 0;
            app->svc_wake[i] = next;
            if (flags & DX_UI_DIRTY) {
                app->dirty = 1;
            }
        }
    }
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
    if (app->tab_changed != NULL) {
        app->tab_changed(next);
    }
    return 1;
}

void dx_app_set_tab(dx_app_t *app, int tab)
{
    if (app->screens == NULL || tab < 0 || tab >= app->count || tab == app->tab) {
        return;
    }
    change_tab(app, tab - app->tab);
}

void dx_app_route(dx_app_t *app, const dx_ui_event_t *ev)
{
    const dx_screen_t *s = active(app);
    unsigned result = DX_UI_IGNORED;
    int changed = 0;

    if (dx_latch_swallow(&app->latch, ev)) {
        return;
    }
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
    dx_latch_note(&app->latch, ev, result, changed);
    dx_ui_sound(dx_ui_sound_for(ev, result, changed));
    wake_services(app);
}

/* The time from ticks to a wake time: 0 when it has passed. */
static uint32_t until(uint32_t ticks, uint32_t wake)
{
    /* The counter wraps at 2^31: see dx_ticks_since in ui.h. */
    return dx_ticks_due(ticks, wake) ? 0 : dx_ticks_since(ticks, wake);
}

/* The wait until the next minute change, the screen's wake time or a
 * service's, from the clocks read now. 0 when one has already passed. */
static uint32_t wait_time(const dx_app_t *app)
{
    uint32_t now = 0;
    int valid = dx_plat_time(&now);
    uint32_t ticks = dx_plat_ticks();
    uint32_t timeout;
    uint32_t t;
    int i;

    if (dx_clock_minute(valid, now) != app->drawn_min) {
        timeout = 0; /* the minute changed during the tick or the draw */
    } else {
        timeout = valid ? dx_clock_wait_ms(now) : DX_WAIT_FOREVER;
    }
    if (app->wake_on && (t = until(ticks, app->wake)) < timeout) {
        timeout = t;
    }
    for (i = 0; i < app->service_count; i++) {
        if (app->svc_on[i] && (t = until(ticks, app->svc_wake[i])) < timeout) {
            timeout = t;
        }
    }
    return timeout;
}

void dx_app_step(dx_app_t *app)
{
    const dx_screen_t *s = active(app);
    uint32_t now = 0;
    int valid = dx_plat_time(&now);
    int32_t minute = dx_clock_minute(valid, now);
    uint32_t ticks = dx_plat_ticks();
    dx_event_t ev;
    dx_ui_event_t uev;

    app->ctx.now_ms = ticks;
    tick_services(app, ticks);
    if (app->wake_on && dx_ticks_due(ticks, app->wake)) {
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
    /* The tick and the draw took time: the wait starts from the clocks
     * read again, so a slow frame does not push the next wake back. */
    if (dx_plat_wait(&ev, wait_time(app)) && dx_ui_translate(&ev, &uev)) {
        dx_app_route(app, &uev);
    }
}

static const dx_service_t *const rom_services[] = { &dx_service_radio, &dx_service_autosave };

static void remember_tab(int tab)
{
    dx_settings_set_tab(dx_content_settings(), tab);
}

void dx_main(void)
{
    static dx_app_t app;
    int32_t volume;

    /* The platform and the theme, then the content, then the saves on
     * top. The level Pip-OS had at start is the volume default. */
    if (dx_app_init(&app, dx_rom_screens, dx_rom_screen_count) != 0) {
        /* A table the header cannot draw: stop here, the log says why. */
        for (;;) {
            dx_event_t ev;

            dx_plat_wait(&ev, DX_WAIT_FOREVER);
        }
    }
    dx_content_load();
    volume = dx_plat_volume(100);
    dx_content_load_saves(volume < 0 ? 100 : (uint8_t)volume);
    dx_radio_attach(dx_content_radio());
    dx_autosave_attach(dx_content_settings(), dx_content_items());
    dx_app_start_services(&app, rom_services,
                          (int)(sizeof(rom_services) / sizeof(rom_services[0])));
    dx_app_set_tab(&app, dx_content_settings()->last_tab);
    app.tab_changed = remember_tab;
    for (;;) {
        dx_app_step(&app);
    }
}
