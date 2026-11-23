/* The main loop, one step at a time against the fake platform. */
#include "rc_test.h"

#include "app.h"
#include "fake_platform.h"
#include "model/nav.h"

/* 2077-10-23T09:47:05Z */
#define T0 3402208025u

TEST(first_frame_then_ready)
{
    dx_app_t app;

    fake_reset();
    dx_app_init(&app);
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK_STR(fake.log, "deitrix: ready\n");
    CHECK(fake_find_text("STAT") != NULL);
    CHECK(fake_find_text("--:--") != NULL);
    /* No clock: nothing to wake up for. */
    CHECK_EQ(fake.last_timeout, DX_WAIT_FOREVER);
}

TEST(no_redraw_without_change)
{
    dx_app_t app;

    fake_reset();
    dx_app_init(&app);
    dx_app_step(&app);
    /* A button that does nothing, then a timeout. */
    fake_push(DX_EV_BUTTON, DX_BTN_UP, DX_PRESS_DOWN);
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
    dx_app_init(&app);
    fake_push(DX_EV_DIAL, 0, 1);
    fake_push(DX_EV_BUTTON, DX_BTN_LEFT, DX_PRESS_DOWN);
    fake_push(DX_EV_BUTTON, DX_BTN_LEFT, DX_PRESS_RELEASE);
    fake_push(DX_EV_DIAL, 0, -2);
    fake_push(DX_EV_DIAL, 0, 5);
    dx_app_step(&app); /* ready, takes the dial +1 */
    dx_app_step(&app); /* INV, takes LEFT */
    dx_app_step(&app); /* STAT, takes the release */
    dx_app_step(&app); /* nothing, takes dial -2 */
    dx_app_step(&app); /* MAP, takes dial +5 */
    dx_app_step(&app); /* still MAP: no redraw */
    CHECK_EQ(app.tab, DX_TAB_MAP);
    CHECK_EQ(fake.presents, 4);
    CHECK_STR(fake.log, "deitrix: ready\ndeitrix: tab=INV\ndeitrix: tab=STAT\n"
                        "deitrix: tab=MAP\n");
}

TEST(clock_shown_and_wait_to_minute)
{
    dx_app_t app;

    fake_reset();
    fake.clock_valid = 1;
    fake.clock = T0;
    dx_app_init(&app);
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
    dx_app_init(&app);
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
    dx_app_init(&app);
    dx_app_step(&app);
    fake.clock_valid = 0;
    fake_clear_draws();
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 1);
    CHECK(fake_find_text("--:--") != NULL);
}

TEST(scheme_colour_used)
{
    dx_app_t app;

    fake_reset();
    fake.fg = 0xffb000u;
    dx_app_init(&app);
    dx_app_step(&app);
    CHECK_EQ(fake_find_text("Not built yet.")->fg, 0x7f5800u);
    CHECK_EQ(fake_find_text("--:--")->fg, 0xffb000u);
}
