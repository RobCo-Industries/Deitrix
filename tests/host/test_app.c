/* The app shell, one step at a time against the fake platform. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "fake_platform.h"
#include "screens/screens.h"
#include "ui/frame.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

/* 2077-10-23T09:47:05Z */
#define T0 3402208025u

static void start(dx_app_t *app)
{
    dx_app_init(app, dx_rom_screens, dx_rom_screen_count);
}

TEST(first_frame_then_ready)
{
    dx_app_t app;

    fake_reset();
    start(&app);
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK_STR(fake.log, "deitrix: ready\n");
    CHECK(fake_find_text("STAT") != NULL);
    CHECK(fake_find_text("--:--") != NULL);
    CHECK(fake_find_text("STATUS") != NULL);
    /* No clock and no screen tick: nothing to wake up for. */
    CHECK_EQ(fake.last_timeout, DX_WAIT_FOREVER);
}

TEST(frame_order_and_clip)
{
    dx_app_t app;
    const char *clip, *body, *present;

    fake_reset();
    start(&app);
    dx_app_step(&app);
    /* Clear first, the body inside the clip, one present last. */
    CHECK(strncmp(fake.draws, "clear bg\n", 9) == 0);
    clip = strstr(fake.draws, "clip 0 22 320 193\n");
    body = strstr(fake.draws, "\"STATUS\"");
    present = strstr(fake.draws, "present\n");
    CHECK(clip != NULL && body != NULL && present != NULL);
    CHECK(clip < body && body < strstr(fake.draws, "clip off\n"));
    CHECK(present != NULL && present[8] == '\0');
}

TEST(no_redraw_without_change)
{
    dx_app_t app;

    fake_reset();
    start(&app);
    dx_app_step(&app);
    /* Keys the placeholder ignores, a release, then a timeout. */
    fake_push(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_DOWN);
    fake_push(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_RELEASE);
    dx_app_step(&app);
    dx_app_step(&app);
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK_EQ(fake.clears, 1);
    CHECK_STR(fake.log, "deitrix: ready\n");
}

TEST(tab_change_redraws_and_logs)
{
    dx_app_t app;

    fake_reset();
    start(&app);
    fake_push(DX_EV_DIAL, 0, 1);
    fake_push(DX_EV_BUTTON, DX_BTN_LEFT, DX_PRESS_DOWN);
    fake_push(DX_EV_BUTTON, DX_BTN_LEFT, DX_PRESS_RELEASE);
    fake_push(DX_EV_DIAL, 0, -2);
    fake_push(DX_EV_DIAL, 0, 5);
    fake_push(DX_EV_BUTTON, DX_BTN_RIGHT, DX_PRESS_REPEAT);
    dx_app_step(&app); /* ready, takes the dial +1 */
    dx_app_step(&app); /* INV, takes LEFT */
    dx_app_step(&app); /* STAT, takes the release */
    dx_app_step(&app); /* nothing, takes dial -2 */
    dx_app_step(&app); /* MAP, takes dial +5: the same tab */
    dx_app_step(&app); /* nothing, takes a RIGHT repeat */
    dx_app_step(&app); /* RADIO */
    CHECK_INT(app.tab, 4);
    CHECK_EQ(fake.presents, 5);
    CHECK_STR(fake.log, "deitrix: ready\ndeitrix: tab=INV\ndeitrix: tab=STAT\n"
                        "deitrix: tab=MAP\ndeitrix: tab=RADIO\n");
}

TEST(clock_shown_and_wait_to_minute)
{
    dx_app_t app;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T0;
    start(&app);
    dx_app_step(&app);
    CHECK(fake_find_text("09:47") != NULL);
    CHECK_EQ(fake.last_timeout, 55000);
}

TEST(minute_change_redraws_once)
{
    dx_app_t app;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T0;
    start(&app);
    dx_app_step(&app);
    fake.clock = T0 + 30; /* same minute */
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK_EQ(fake.last_timeout, 25000);
    fake.clock = T0 + 55; /* 09:48:00 */
    fake_clear_draws();
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK(fake_find_text("09:48") != NULL);
    CHECK_EQ(fake.last_timeout, 60000);
    /* A minute change is not a tab change: no log line. */
    CHECK_STR(fake.log, "deitrix: ready\n");
}

TEST(clock_lost_shows_dashes)
{
    dx_app_t app;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T0;
    start(&app);
    dx_app_step(&app);
    fake.clock_valid = 0;
    fake_clear_draws();
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK(fake_find_text("--:--") != NULL);
}

TEST(init_refuses_a_bad_table)
{
    static const dx_screen_t ok = { "OK", NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    static const dx_screen_t big = { "TOOLONG", NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    static const dx_screen_t *const bad[] = { &ok, &big };
    static const dx_screen_t *const nine[] = { &ok, &ok, &ok, &ok, &ok, &ok, &ok, &ok, &ok };
    dx_app_t app;

    fake_reset();
    CHECK_INT(dx_app_init(&app, bad, 2), -1);
    CHECK_STR(fake.log, "deitrix: refused: tab names need 1 to 5 letters\n");
    fake_reset();
    CHECK_INT(dx_app_init(&app, nine, 9), -1);
    CHECK_STR(fake.log, "deitrix: refused: the tab table needs 1 to 8 screens\n");
    fake_reset();
    CHECK_INT(dx_app_init(&app, nine, 8), 0);
    CHECK_STR(fake.log, "");
}

TEST(init_refuses_null_pointers)
{
    static const dx_screen_t ok = { "OK", NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    static const dx_screen_t *const first[] = { NULL, &ok, &ok };
    static const dx_screen_t *const middle[] = { &ok, NULL, &ok };
    static const dx_screen_t *const last[] = { &ok, &ok, NULL };
    static const dx_screen_t *const *const tables[] = { first, middle, last };
    static const dx_screen_t *const one[] = { &ok };
    dx_app_t app;
    unsigned i;

    fake_reset();
    CHECK_INT(dx_app_init(&app, NULL, 3), -1);
    CHECK_STR(fake.log, "deitrix: refused: the tab table needs 1 to 8 screens\n");
    CHECK_INT(app.count, 0);
    for (i = 0; i < 3; i++) {
        fake_reset();
        CHECK_INT(dx_app_init(&app, tables[i], 3), -1);
        CHECK_STR(fake.log, "deitrix: refused: the tab table has an empty entry\n");
        CHECK_INT(app.count, 0);
        CHECK(app.screens == NULL);
    }
    fake_reset();
    CHECK_INT(dx_app_init(NULL, one, 1), -1);
    CHECK_STR(fake.log, "deitrix: refused: no app\n");
}

TEST(scheme_colour_used)
{
    dx_app_t app;

    fake_reset();
    fake.fg = 0xffb000u;
    start(&app);
    dx_app_step(&app);
    CHECK_EQ(fake_find_text("ATTRS")->fg, 0x7f5800u);
    CHECK_EQ(fake_find_text("--:--")->fg, 0xffb000u);
}

/* ---- Routing, with a scripted screen ------------------------------------ */

typedef struct probe {
    char trace[256];
    unsigned answer[8]; /* per DX_UI_* type */
    uint32_t next_wake;
    int ticks;
} probe_t;

static probe_t pa, pb;

static void note(probe_t *p, const char *s)
{
    tfmt(p->trace + strlen(p->trace), sizeof(p->trace) - strlen(p->trace), "%s", s);
}

static void p_enter(void *st)
{
    note(st, "enter ");
}

static void p_leave(void *st)
{
    note(st, "leave ");
}

static unsigned p_event(void *st, const dx_ui_event_t *ev)
{
    static const char *const n[] = { "U", "D", "L", "R", "S", "B", "J" };
    probe_t *p = st;

    note(p, n[ev->type]);
    note(p, " ");
    return p->answer[ev->type];
}

static uint32_t p_tick(void *st, uint32_t now, unsigned *flags)
{
    probe_t *p = st;

    (void)now;
    p->ticks++;
    *flags |= DX_UI_DIRTY;
    return p->next_wake;
}

static const char *p_hint(void *st)
{
    (void)st;
    return "PROBE HINT";
}

static const dx_screen_t sa = { "AAA", p_enter, p_leave, p_event, NULL, NULL, p_hint, &pa };
static const dx_screen_t sb = { "BBB", p_enter, p_leave, p_event, p_tick, NULL, NULL, &pb };
static const dx_screen_t sc = { "CCC", NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static const dx_screen_t *const table[] = { &sa, &sb, &sc };

static void probe_start(dx_app_t *app)
{
    fake_reset();
    memset(&pa, 0, sizeof(pa));
    memset(&pb, 0, sizeof(pb));
    dx_app_init(app, table, 3);
}

static void route(dx_app_t *app, int type, int32_t delta)
{
    dx_ui_event_t ev = { type, 0, delta };

    dx_app_route(app, &ev);
}

TEST(route_dial_skips_the_screen)
{
    dx_app_t app;

    probe_start(&app);
    route(&app, DX_UI_DIAL, 1);
    CHECK_INT(app.tab, 1);
    CHECK_STR(pa.trace, "enter leave ");
    CHECK_STR(pb.trace, "enter ");
    route(&app, DX_UI_DIAL, -2);
    CHECK_INT(app.tab, 2);
    route(&app, DX_UI_DIAL, 3); /* back to the same tab: nothing */
    CHECK_INT(app.tab, 2);
    CHECK_STR(pb.trace, "enter leave ");
    CHECK_INT(fake.tone_count, 2); /* two MOVE tones */
}

TEST(route_left_right_ignored_change_tab)
{
    dx_app_t app;

    probe_start(&app);
    route(&app, DX_UI_RIGHT, 0);
    CHECK_INT(app.tab, 1);
    CHECK_STR(pa.trace, "enter R leave ");
    route(&app, DX_UI_LEFT, 0);
    CHECK_INT(app.tab, 0);
    route(&app, DX_UI_LEFT, 0); /* wraps */
    CHECK_INT(app.tab, 2);
    route(&app, DX_UI_RIGHT, 0); /* a screen with no event handler */
    CHECK_INT(app.tab, 0);
}

TEST(route_left_right_handled_stay)
{
    dx_app_t app;

    probe_start(&app);
    pa.answer[DX_UI_LEFT] = DX_UI_HANDLED | DX_UI_DIRTY;
    pa.answer[DX_UI_RIGHT] = DX_UI_HANDLED;
    route(&app, DX_UI_LEFT, 0);
    route(&app, DX_UI_RIGHT, 0);
    CHECK_INT(app.tab, 0);
    CHECK_INT(app.dirty, 1);
    CHECK_STR(pa.trace, "enter L R ");
}

TEST(route_back_and_others_ignored_do_nothing)
{
    dx_app_t app;

    probe_start(&app);
    route(&app, DX_UI_BACK, 0);
    route(&app, DX_UI_UP, 0);
    route(&app, DX_UI_SELECT, 0);
    CHECK_INT(app.tab, 0);
    CHECK_INT(app.dirty, 0);
    CHECK_STR(pa.trace, "enter B U S ");
    /* Each is a DENY. */
    CHECK_INT(fake.tone_count, 3);
}

TEST(hint_and_tick)
{
    dx_app_t app;

    probe_start(&app);
    dx_app_step(&app);
    CHECK(fake_find_text("PROBE HINT") != NULL);
    /* B ticks at once on enter, then at its next wake time. */
    pb.next_wake = fake.ticks + 250;
    fake_push(DX_EV_DIAL, 0, 1);
    dx_app_step(&app); /* takes the dial */
    fake_clear_draws();
    dx_app_step(&app); /* ticks, draws B, waits for the wake */
    CHECK_INT(pb.ticks, 1);
    CHECK(fake_find_text(DX_FRAME_HINT) != NULL);
    CHECK_EQ(fake.last_timeout, 250);
    fake.ticks += 100;
    dx_app_step(&app); /* not due yet: no tick, no redraw */
    CHECK_INT(pb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 150);
    CHECK_EQ(fake.presents, 1);
    pb.next_wake = 0;
    fake.ticks += 150;
    dx_app_step(&app); /* due: ticks, dirty, redraws, then no wake */
    CHECK_INT(pb.ticks, 2);
    CHECK_EQ(fake.presents, 2);
    CHECK_EQ(fake.last_timeout, DX_WAIT_FOREVER);
}

/* ---- The tick counter wraps at 2^31 ------------------------------------- */

/* Starts on B with the counter at `start`, after B's first tick. */
static void wrap_start(dx_app_t *app, uint32_t start, uint32_t first_wake)
{
    probe_start(app);
    dx_app_step(app);
    fake.ticks = start;
    pb.next_wake = first_wake;
    fake_push(DX_EV_DIAL, 0, 1);
    dx_app_step(app); /* takes the dial */
    dx_app_step(app); /* B ticks on enter */
    CHECK_INT(pb.ticks, 1);
    pb.next_wake = 0;
}

TEST(wake_after_the_wrap_is_not_early)
{
    dx_app_t app;

    /* 0x200 ms after 0x7fffff00 is tick 0x100. */
    wrap_start(&app, 0x7fffff00u, dx_ticks_add(0x7fffff00u, 0x200));
    CHECK_EQ(fake.last_timeout, 0x200);
    fake.ticks = 0x7fffffffu;
    dx_app_step(&app);
    CHECK_INT(pb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 0x101);
    fake.ticks = 0xffu;
    dx_app_step(&app);
    CHECK_INT(pb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 1);
    fake.ticks = 0x100u;
    dx_app_step(&app);
    CHECK_INT(pb.ticks, 2);
}

TEST(wake_before_the_wrap_is_not_late)
{
    dx_app_t app;

    wrap_start(&app, 0x7fffff00u, dx_ticks_add(0x7fffff00u, 0xf0));
    CHECK_EQ(fake.last_timeout, 0xf0);
    /* The counter wrapped past the wake: due now. */
    fake.ticks = 0x10u;
    dx_app_step(&app);
    CHECK_INT(pb.ticks, 2);
}

TEST(wake_on_tick_zero_still_fires)
{
    dx_app_t app;

    /* 0x100 ms after 0x7fffff00 is tick 0, nudged to 1. */
    wrap_start(&app, 0x7fffff00u, dx_ticks_add(0x7fffff00u, 0x100));
    CHECK_INT(app.wake_on, 1);
    CHECK_EQ(app.wake, 1u);
    CHECK_EQ(fake.last_timeout, 0x101);
    fake.ticks = 0;
    dx_app_step(&app);
    CHECK_INT(pb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 1);
    fake.ticks = 1;
    dx_app_step(&app);
    CHECK_INT(pb.ticks, 2);
}

/* ---- The wait after a slow draw ----------------------------------------- */

TEST(slow_draw_does_not_delay_the_wake)
{
    dx_app_t app;

    probe_start(&app);
    fake_push(DX_EV_DIAL, 0, 1);
    dx_app_step(&app); /* draws A, takes the dial */
    pb.next_wake = fake.ticks + 250;
    fake.draw_ms = 300; /* each frame takes longer than the wait */
    dx_app_step(&app); /* ticks B, draws it: the wake passed meanwhile */
    CHECK_INT(pb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 0);
    pb.next_wake = 0;
    fake.draw_ms = 0;
    dx_app_step(&app); /* the next step ticks at once */
    CHECK_INT(pb.ticks, 2);
    CHECK_EQ(fake.last_timeout, DX_WAIT_FOREVER);
}

TEST(short_draw_waits_the_rest)
{
    dx_app_t app;

    probe_start(&app);
    fake_push(DX_EV_DIAL, 0, 1);
    dx_app_step(&app);
    pb.next_wake = fake.ticks + 250;
    fake.draw_ms = 100;
    dx_app_step(&app); /* ticks, draws in 100 ms */
    CHECK_INT(pb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 150);
}

TEST(slow_draw_across_the_minute)
{
    dx_app_t app;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T0 + 50; /* 09:47:55 */
    start(&app);
    fake.draw_s = 6; /* the first frame ends at 09:48:01 */
    fake.draw_ms = 6000;
    dx_app_step(&app);
    CHECK(fake_find_text("09:47") != NULL);
    CHECK_EQ(fake.last_timeout, 0);
    fake.draw_s = 0;
    fake.draw_ms = 0;
    fake_clear_draws();
    dx_app_step(&app); /* draws 09:48 without a wait between */
    CHECK_EQ(fake.presents, 1);
    CHECK(fake_find_text("09:48") != NULL);
    CHECK_EQ(fake.last_timeout, 59000);
}

TEST(short_draw_waits_to_the_minute)
{
    dx_app_t app;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T0; /* 09:47:05 */
    start(&app);
    fake.draw_s = 3;
    dx_app_step(&app);
    CHECK_EQ(fake.last_timeout, 52000);
}

/* ---- Services ------------------------------------------------------------ */

/* A service that counts its ticks and asks for a fixed interval. */
static struct {
    int inits, ticks, stops;
    uint32_t every; /* 0: no wake */
    uint32_t last;
} sva, svb;

static void sa_init(uint32_t now) { (void)now; sva.inits++; }
static uint32_t sa_tick(uint32_t now, unsigned *flags)
{
    (void)flags;
    sva.ticks++;
    sva.last = now;
    return sva.every ? dx_ticks_add(now, sva.every) : 0;
}
static void sa_stop(void) { sva.stops++; }
static uint32_t sb_tick(uint32_t now, unsigned *flags)
{
    (void)flags;
    svb.ticks++;
    svb.last = now;
    return svb.every ? dx_ticks_add(now, svb.every) : 0;
}

static const dx_service_t svc_a = { "a", sa_init, sa_tick, sa_stop };
static const dx_service_t svc_b = { "b", NULL, sb_tick, NULL };
static const dx_service_t *const two[] = { &svc_a, &svc_b };

static void svc_start(dx_app_t *app, uint32_t ticks, uint32_t a, uint32_t b)
{
    fake_reset();
    fake.ticks = ticks;
    memset(&sva, 0, sizeof(sva));
    memset(&svb, 0, sizeof(svb));
    sva.every = a;
    svb.every = b;
    start(app);
    CHECK_INT(dx_app_start_services(app, two, 2), 0);
    CHECK_INT(sva.inits, 1);
}

TEST(services_tick_and_the_wait_takes_the_earliest)
{
    dx_app_t app;

    svc_start(&app, 1000, 300, 700);
    dx_app_step(&app); /* both tick at once */
    CHECK_INT(sva.ticks, 1);
    CHECK_INT(svb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 300);
    fake.ticks = 1299;
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 1);
    CHECK_EQ(fake.last_timeout, 1);
    fake.ticks = 1300;
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 2);
    CHECK_INT(svb.ticks, 1);
    CHECK_EQ(fake.last_timeout, 300); /* a at 1600, b at 1700 */
    fake.ticks = 1700;
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 3);
    CHECK_INT(svb.ticks, 2);
    CHECK_EQ(fake.last_timeout, 300); /* a at 2000, b at 2400 */
}

TEST(services_without_a_wake_and_with_the_clock)
{
    dx_app_t app;

    svc_start(&app, 1000, 0, 0);
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 1);
    CHECK_EQ(fake.last_timeout, DX_WAIT_FOREVER);
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 1);
    /* A minute change 5 s away comes before a service 9 s away. */
    svc_start(&app, 1000, 9000, 0);
    fake.clock_valid = 1;
    fake.clock = T0; /* 55 s before the minute */
    dx_app_step(&app);
    CHECK_EQ(fake.last_timeout, 9000);
    fake.clock = T0 + 50;
    dx_app_step(&app);
    CHECK_EQ(fake.last_timeout, 5000);
}

TEST(services_tick_after_each_event)
{
    dx_app_t app;

    svc_start(&app, 1000, 5000, 0);
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 1);
    fake_push(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_DOWN);
    dx_app_step(&app); /* takes the event */
    fake.ticks = 1010;
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 2);
    CHECK_INT(svb.ticks, 2);
    CHECK_INT(sva.last, 1010);
}

TEST(services_across_the_tick_wrap)
{
    dx_app_t app;

    svc_start(&app, 0x7fffff00u, 0x200, 0);
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 1);
    CHECK_EQ(fake.last_timeout, 0x200);
    fake.ticks = 0xffu; /* wrapped, 1 ms early */
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 1);
    CHECK_EQ(fake.last_timeout, 1);
    fake.ticks = 0x100u;
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 2);
    CHECK_EQ(fake.last_timeout, 0x200);
}

TEST(services_table_checked_and_stopped)
{
    dx_app_t app;
    const dx_service_t *const bad[] = { &svc_a, NULL };
    static const dx_service_t no_tick = { "x", NULL, NULL, NULL };
    const dx_service_t *const bad2[] = { &no_tick };

    svc_start(&app, 1000, 0, 0);
    CHECK_INT(dx_app_start_services(&app, bad, 2), -1);
    CHECK_INT(dx_app_start_services(&app, bad2, 1), -1);
    CHECK_INT(dx_app_start_services(&app, two, DX_SERVICES_MAX + 1), -1);
    CHECK_INT(dx_app_start_services(&app, NULL, 1), -1);
    CHECK_INT(dx_app_start_services(&app, NULL, 0), 0);
    CHECK_INT(dx_app_start_services(&app, two, 2), 0);
    dx_app_stop_services(&app);
    CHECK_INT(sva.stops, 1);
    dx_app_step(&app);
    CHECK_INT(sva.ticks, 0);
}

static int told_tab = -1;

static void tell(int tab)
{
    told_tab = tab;
}

TEST(set_tab_before_the_first_frame)
{
    dx_app_t app;

    fake_reset();
    start(&app);
    app.tab_changed = tell;
    dx_app_set_tab(&app, 4);
    dx_app_set_tab(&app, 9);
    dx_app_set_tab(&app, -1);
    CHECK_INT(app.tab, 4);
    CHECK_INT(told_tab, 4);
    dx_app_step(&app);
    CHECK_STR(fake.log, "deitrix: ready\n");
    CHECK(fake_find_text("RADIO") != NULL);
    fake_push(DX_EV_DIAL, 0, 1);
    dx_app_step(&app);
    CHECK_INT(told_tab, 0);
}

/* ---- The radio service and the redraw ------------------------------------ */

#include "content.h"
#include "services/services.h"

static const dx_service_t *const radio_only[] = { &dx_service_radio };

/* A card with two tracks: 1 plays once (2.5 s), 2 loops. The app starts
 * on `tab` with the radio service, and draws its first frame. */
static void radio_start(dx_app_t *app, int tab)
{
    fake_reset();
    fake_add_text("/PIPBOY/DATA/RADIO.INI",
                  "version = 1\n"
                  "[station.1]\nname = Once\nkind = pcm\nfile = O.PCM\nloop = no\n"
                  "[station.2]\nname = Loop\nkind = pcm\nfile = L.PCM\n");
    fake_add_fill("/PIPBOY/DATA/RADIO/O.PCM", 20000, 128);
    fake_add_fill("/PIPBOY/DATA/RADIO/L.PCM", 80000, 128);
    dx_ui_sound_block(0);
    dx_content_load();
    dx_settings_set_station(dx_content_settings(), 0); /* SELECT plays station 1 */
    dx_radio_attach(dx_content_radio());
    start(app);
    dx_app_set_tab(app, tab);
    CHECK_INT(dx_app_start_services(app, radio_only, 1), 0);
    dx_app_step(app);
    CHECK_EQ(fake.presents, 1);
}

/* One step, then the clock moves to the wait's end, as the board would. */
static void radio_step(dx_app_t *app)
{
    dx_app_step(app);
    if (fake.last_timeout != DX_WAIT_FOREVER) {
        fake.ticks = dx_ticks_add(fake.ticks, fake.last_timeout);
    }
}

/* Steps until the radio is off, at most `max` steps: the count left. */
static int radio_until_off(dx_app_t *app, int max)
{
    while (max > 0 && dx_radio_state() != DX_RADIO_OFF) {
        fake_clear_draws();
        radio_step(app);
        max--;
    }
    return max;
}

TEST(radio_end_redraws_the_radio_tab)
{
    dx_app_t app;
    int wakes = 0;

    radio_start(&app, 4);
    fake_clear_draws();
    route(&app, DX_UI_SELECT, 0); /* station 1 */
    radio_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK(fake_find_text("Playing Once") != NULL);
    /* The stream wakes every 250 ms with nothing to show: no frame. */
    while (wakes < 20 && dx_radio_state() == DX_RADIO_PCM) {
        fake_clear_draws();
        radio_step(&app);
        if (dx_radio_state() == DX_RADIO_PCM) {
            CHECK_EQ(fake.presents, 0);
        }
        wakes++;
    }
    /* The track ended in the last step: that step drew once, off. */
    CHECK_INT(dx_radio_state(), DX_RADIO_OFF);
    CHECK(wakes > 4);
    CHECK_EQ(fake.presents, 1);
    CHECK(fake_find_text("Off") != NULL);
    CHECK(fake_find_text("Playing Once") == NULL);
    CHECK(strstr(fake.log, "deitrix: radio shows Playing Once\n"
                           "deitrix: radio shows Off\n") != NULL);
    fake_clear_draws();
    radio_step(&app);
    CHECK_EQ(fake.presents, 0);
}

TEST(radio_end_redraws_another_tab_once)
{
    dx_app_t app;

    radio_start(&app, 4);
    route(&app, DX_UI_SELECT, 0);
    route(&app, DX_UI_DIAL, -4); /* to STAT, the radio plays on */
    radio_step(&app);
    CHECK_INT(app.tab, 0);
    CHECK_INT(dx_radio_state(), DX_RADIO_PCM);
    CHECK(radio_until_off(&app, 20) > 0);
    /* Any service asking for a redraw redraws the tab shown. */
    CHECK_EQ(fake.presents, 1);
    CHECK(fake_find_text("STAT") != NULL);
}

TEST(radio_underrun_redraws_once)
{
    dx_app_t app;
    int i;

    radio_start(&app, 4);
    route(&app, DX_UI_SELECT, 0);
    route(&app, DX_UI_SELECT, 0); /* station 2, the loop */
    radio_step(&app);
    CHECK(fake_find_text("Playing Loop") != NULL);
    fake_clear_draws();
    for (i = 0; i < 12; i++) {
        radio_step(&app);
    }
    CHECK_EQ(fake.presents, 0);
    CHECK_INT(dx_radio_underruns(), 0);
    /* The ROM stalls for 3 s: the queue runs dry. */
    fake.ticks = dx_ticks_add(fake.ticks, 3000);
    radio_step(&app);
    CHECK_INT(dx_radio_underruns(), 1);
    CHECK_EQ(fake.presents, 1);
    fake_clear_draws();
    for (i = 0; i < 8; i++) {
        radio_step(&app);
    }
    CHECK_EQ(fake.presents, 0);
    dx_radio_stop();
}
