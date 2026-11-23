/* The STAT tab: the name fit, its three sub-tabs, the keys and the ABOUT tick. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "content.h"
#include "fake_platform.h"
#include "screens/screens.h"
#include "screens/stat_draw.h"
#include "ui/frame.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))
#define PROFILE_PATH "/PIPBOY/DATA/PROFILE.INI"
#define DENY_HZ 180

/* ---- The name ----------------------------------------------------------- */

TEST(name_fit)
{
    /* 312 px: the body less 4 px each side. */
    CHECK_INT(dx_stat_name_unit("Field kit 7", 312), 4);
    /* 13 glyphs: 13 * 24 = 312 px, plus the stroke's extra pixel. */
    CHECK_INT(dx_stat_name_unit("ABCDEFGHIJKL", 312), 4);
    CHECK_INT(dx_stat_name_unit("ABCDEFGHIJKLM", 312), 3);
    CHECK_INT(dx_stat_name_unit("ABCDEFGHIJKLMNOPQ", 312), 3);
    CHECK_INT(dx_stat_name_unit("ABCDEFGHIJKLMNOPQR", 312), 0);
    /* A character the stroke font lacks: the normal font. */
    CHECK_INT(dx_stat_name_unit("Kit #2", 312), 0);
    CHECK_INT(dx_stat_name_unit("", 312), 0);
    CHECK_INT(dx_stat_name_unit(NULL, 312), 0);
}

/* ---- The screen through the app shell ----------------------------------- */

static const char FULL[] =
    "version = 1\n"
    "[owner]\n"
    "name = Abcdefghijklmnopqrstuvwx\n"
    "line = The longest owner line takes 38 chars\n"
    "[gauges]\n"
    "g1 = 9999,9999,Gauge one longes\n"
    "g2 = 0,9999,Gauge two longes\n"
    "g3 = 1,1,Gauge three long\n"
    "g4 = 5000,9999,Gauge four longe\n"
    "g5 = 7,9,Gauge five longe\n"
    "g6 = 9999,9999,Gauge six longest\n"
    "[attrs]\n"
    "a1 = 99,Attribute one 16\n"
    "a2 = 1,Attribute two 16\n"
    "a3 = 2,Attribute 3 16ch\n"
    "a4 = 3,Attribute 4 16ch\n"
    "a5 = 4,Attribute 5 16ch\n"
    "a6 = 5,Attribute 6 16ch\n"
    "a7 = 6,Attribute 7 16ch\n"
    "a8 = 7,Attribute 8 16ch\n"
    "a9 = 8,Attribute 9 16ch\n"
    "a10 = 9,Attribute 10 16c\n"
    "a11 = 10,Attribute 11 16c\n"
    "a12 = 0,Attribute 12 16c\n"
    "[about]\n"
    "l1 = About line 1 is as long as allowed..\n"
    "l2 = About line 2 is as long as allowed..\n"
    "l3 = About line 3 is as long as allowed..\n"
    "l4 = About line 4 is as long as allowed..\n"
    "l5 = About line 5 is as long as allowed..\n"
    "l6 = About line 6 is as long as allowed..\n"
    "l7 = About line 7 is as long as allowed..\n"
    "l8 = About line 8 is as long as allowed..\n";

static const char MINIMAL[] = "version = 1\n[owner]\nname = Kit\n";

static dx_app_t app;

static void boot(const char *profile)
{
    fake_reset();
    fake.sysinfo.abi_version = 0x00010002u;
    fake.sysinfo.kernel_version = 0x00070100u;
    fake.sysinfo.soc = 0x2836u;
    fake.sysinfo.mem_total = 1006632960u;
    if (profile != NULL) {
        fake_add_text(PROFILE_PATH, profile);
    }
    dx_content_load();
    dx_stat_reset();
    fake.log[0] = '\0';
    CHECK_INT(dx_app_init(&app, dx_rom_screens, dx_rom_screen_count), 0);
    dx_app_step(&app);
    CHECK_STR(fake.log, "deitrix: ready\n");
    fake.log[0] = '\0';
}

/* Routes one key, then runs a step so the shell redraws. */
static void key(int type)
{
    dx_ui_event_t ev = { type, 0, 0 };

    if (type == DX_UI_DIAL) {
        ev.delta = 1;
    }
    dx_app_route(&app, &ev);
    dx_app_step(&app);
}

/* Draws the body alone into a fresh draw list. */
static void draw_body(void)
{
    dx_ctx_t c = fake_ctx();

    fake_clear_draws();
    dx_screen_stat.draw(NULL, &c, dx_frame_body());
}

static int body_inside(void)
{
    return fake_points_inside(dx_frame_body());
}

static const char *hint(void)
{
    return dx_screen_stat.hint(NULL);
}

TEST(status_full_profile)
{
    const fake_text_t *t;

    boot(FULL);
    draw_body();
    CHECK(body_inside());
    /* 24 characters do not fit the stroke font: the normal font, centred. */
    t = fake_find_text("Abcdefghijklmnopqrstuvwx");
    CHECK(t != NULL && t->scale == 1);
    CHECK_INT(t->x, (320 - 24 * 8) / 2);
    CHECK(fake_find_text("The longest owner line takes 38 chars") != NULL);
    /* Six gauges, the labels in one column, the numbers in another. */
    CHECK(fake_find_text("Gauge one longes") != NULL);
    t = fake_find_text("Gauge six longes");
    CHECK(t != NULL && t->x == 4 && t->y == 42 + 66 + 5 * 18);
    CHECK_INT(t->y + DX_ROW_H - 1 <= DX_BODY_BOTTOM, 1);
    t = fake_find_text("1/1");
    CHECK(t != NULL && t->x == 316 - 3 * 8);
    CHECK_INT(fake_find_text("0/9999")->x, 316 - 6 * 8);
    CHECK_STR(hint(), "RIGHT: ATTRS   LEFT or DIAL: tab");
    CHECK_STR(fake.log, "");
}

TEST(status_draw_list_sample)
{
    static const char sample[] = "version = 1\n[owner]\nname = Kit 7\nline = Spare\n"
                                 "[gauges]\ng1 = 3,4,Water\n";

    boot(sample);
    draw_body();
    /* The sub-tabs, the name in the stroke font, the line, the rule and
     * one gauge. Only the first lines: the strokes are many. */
    CHECK(strncmp(fake.draws,
                  "text 4 23 \"STATUS\" fg none s1\n"
                  "rect 4 39 48 2 fg fill\n"
                  "text 68 23 \"ATTRS\" dim none s1\n"
                  "text 124 23 \"ABOUT\" dim none s1\n"
                  "line 0 41 319 41 faint\n",
                  strlen("text 4 23 \"STATUS\" fg none s1\n"
                         "rect 4 39 48 2 fg fill\n"
                         "text 68 23 \"ATTRS\" dim none s1\n"
                         "text 124 23 \"ABOUT\" dim none s1\n"
                         "line 0 41 319 41 faint\n")) == 0);
    CHECK(strstr(fake.draws, "text 140 80 \"Spare\" dim none s1\n") != NULL);
    CHECK(strstr(fake.draws, "line 4 102 315 102 faint\n") != NULL);
    CHECK(strstr(fake.draws, "text 4 108 \"Water\" dim none s1\n") != NULL);
    CHECK(strstr(fake.draws, "text 292 108 \"3/4\" fg none s1\n") != NULL);
    /* "KIT 7" at unit 4: 5 glyphs of 24 px, centred. */
    CHECK(strstr(fake.draws, "\"Kit 7\"") == NULL);
    CHECK(body_inside());
}

TEST(attrs_full_list_scrolls)
{
    int i;

    boot(FULL);
    key(DX_UI_RIGHT);
    CHECK_INT(app.tab, 0);
    CHECK_STR(fake.log, "deitrix: stat shows ATTRS sel=0 top=0\n");
    CHECK_STR(hint(), "UP/DOWN: move   LEFT/RIGHT: page");
    draw_body();
    CHECK(body_inside());
    /* Ten rows of 16 px fit under the sub-tabs. */
    CHECK(fake_find_text("Attribute 10 16c") != NULL);
    CHECK(fake_find_text("Attribute 11 16c") == NULL);
    CHECK_INT(fake_find_text("99")->x, 320 - 4 - DX_SCROLLBAR_W - 2 - 16);
    fake.log[0] = '\0';
    for (i = 1; i < 12; i++) {
        key(DX_UI_DOWN);
    }
    CHECK(strstr(fake.log, "deitrix: stat shows ATTRS sel=11 top=2\n") != NULL);
    draw_body();
    CHECK(body_inside());
    CHECK(fake_find_text("Attribute 12 16c") != NULL);
    CHECK(fake_find_text("Attribute 3 16ch") != NULL);
    CHECK(fake_find_text("Attribute two 16") == NULL);
    /* DOWN at the end and SELECT do nothing and deny. */
    fake.log[0] = '\0';
    fake.tone_count = 0;
    fake.presents = 0;
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    CHECK_STR(fake.log, "");
    CHECK_INT(fake.presents, 0);
    CHECK_INT(fake.tone_count, 2);
    CHECK_INT(fake.tones[0][0], DENY_HZ);
    CHECK_INT(fake.tones[1][0], DENY_HZ);
    for (i = 1; i < 12; i++) {
        key(DX_UI_UP);
    }
    CHECK(strstr(fake.log, "deitrix: stat shows ATTRS sel=0 top=0\n") != NULL);
    fake.log[0] = '\0';
    key(DX_UI_UP);
    CHECK_STR(fake.log, "");
}

TEST(about_full_profile)
{
    boot(FULL);
    fake.ticks = 90061500u;
    key(DX_UI_RIGHT);
    fake.log[0] = '\0';
    key(DX_UI_RIGHT);
    CHECK_STR(fake.log, "deitrix: stat about Deitrix=0.1.0 | Pip-OS=7.1.0 | ABI=1.2 | "
                        "SoC=BCM2836 | RAM=960 MiB | Uptime=1d 01:01:01 | Time=no clock\n");
    CHECK_STR(hint(), "LEFT: ATTRS   RIGHT or DIAL: tab");
    draw_body();
    CHECK(body_inside());
    CHECK_INT(fake_find_text("Deitrix")->y, 42 + 4);
    CHECK_INT(fake_find_text("0.1.0")->x, 4 + 9 * 8);
    CHECK_INT(fake_find_text("no clock")->y, 42 + 4 + 6 * 16);
    /* The about lines fill the rest of the body, never past it. */
    CHECK(fake_find_text("About line 3 is as long as allowed..") != NULL);
    CHECK(fake_find_text("About line 4 is as long as allowed..") == NULL);
    CHECK_INT(fake_find_text("About line 3 is as long as allowed..")->y + DX_ROW_H - 1 <=
                  DX_BODY_BOTTOM,
              1);
    /* RIGHT on the last sub-tab: the shell moves to INV. */
    key(DX_UI_RIGHT);
    CHECK_INT(app.tab, 1);
}

TEST(about_clock_and_no_sysinfo)
{
    boot(MINIMAL);
    fake.sysinfo_fail = 1;
    fake.clock_valid = 1;
    fake.clock = 3402208025u;
    fake.ticks = 5000;
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    CHECK(strstr(fake.log, "Pip-OS=unknown | ABI=unknown | SoC=unknown | RAM=unknown | "
                           "Uptime=0d 00:00:05 | Time=2077-10-23 09:47:05 UTC\n") != NULL);
    draw_body();
    CHECK(body_inside());
    /* No about lines: no rule under the rows. */
    CHECK(strstr(fake.draws, "line 4 ") == NULL);
}

TEST(minimal_profile)
{
    boot(MINIMAL);
    draw_body();
    CHECK(body_inside());
    CHECK(fake_find_text("Kit") == NULL); /* the stroke font draws it */
    CHECK(strstr(fake.draws, "line 4 102") == NULL);
    key(DX_UI_RIGHT);
    draw_body();
    CHECK(fake_find_text("No attributes in the profile.") != NULL);
    CHECK_STR(hint(), "LEFT/RIGHT: page   DIAL: tab");
    fake.tone_count = 0;
    key(DX_UI_DOWN);
    CHECK_INT(fake.tones[0][0], DENY_HZ);
}

TEST(no_profile)
{
    const fake_text_t *t;

    boot(NULL);
    draw_body();
    t = fake_find_text("No profile on the card.");
    CHECK(t != NULL && t->fg == fake.theme.dim);
    CHECK_INT(t->x, (320 - 23 * 8) / 2);
    CHECK(body_inside());
    key(DX_UI_RIGHT);
    draw_body();
    CHECK(fake_find_text("No profile on the card.") != NULL);
    CHECK(fake_find_text("No attributes in the profile.") == NULL);
    key(DX_UI_RIGHT);
    draw_body();
    CHECK(fake_find_text("BCM2836") != NULL);
}

TEST(broken_profile)
{
    boot("version = 2\n[owner]\nname = Kit\n");
    draw_body();
    CHECK(fake_find_text("The profile was not read.") != NULL);
    CHECK(fake_find_text("PROFILE.INI bad version") != NULL);
    CHECK(body_inside());
    boot("version = 1\n[owner\n");
    draw_body();
    CHECK(fake_find_text("PROFILE.INI line 2") != NULL);
}

TEST(status_ignores_keys)
{
    boot(FULL);
    fake.tone_count = 0;
    fake.presents = 0;
    key(DX_UI_UP);
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    key(DX_UI_BACK);
    CHECK_INT(fake.presents, 0);
    CHECK_INT(fake.tone_count, 4);
    CHECK_INT(app.tab, 0);
    /* LEFT on the first sub-tab: the shell wraps to the last tab. */
    key(DX_UI_LEFT);
    CHECK_INT(app.tab, 4);
}

TEST(sub_tab_kept_across_tabs)
{
    dx_ui_event_t back = { DX_UI_DIAL, 0, -1 };

    boot(FULL);
    key(DX_UI_RIGHT);
    key(DX_UI_DOWN);
    key(DX_UI_DIAL); /* to INV */
    CHECK_INT(app.tab, 1);
    fake.log[0] = '\0';
    dx_app_route(&app, &back);
    dx_app_step(&app);
    CHECK_INT(app.tab, 0);
    CHECK_STR(fake.log, "deitrix: stat shows ATTRS sel=1 top=0\ndeitrix: tab=STAT\n");
    /* Back to STATUS from another sub-tab logs it. */
    fake.log[0] = '\0';
    key(DX_UI_LEFT);
    CHECK_STR(fake.log, "deitrix: stat shows STATUS\n");
}

TEST(about_tick)
{
    unsigned flags = 0;

    boot(FULL);
    /* Not on ABOUT: no tick. */
    CHECK_INT(dx_screen_stat.tick(NULL, 1500, &flags), 0);
    CHECK_INT(flags, 0);
    CHECK_INT(app.wake_on, 0);
    fake.ticks = 1500;
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    /* The shell ticked after the event: wake at the next whole second. */
    CHECK_INT(app.wake_on, 1);
    CHECK_INT(app.wake, 2000);
    CHECK(fake_find_text("0d 00:00:01") != NULL);
    /* Before the second: nothing changed, no redraw. */
    CHECK_INT(dx_screen_stat.tick(NULL, 1999, &flags), 2000);
    CHECK_INT(flags, 0);
    /* At the second: the uptime changed. */
    CHECK_INT(dx_screen_stat.tick(NULL, 2000, &flags), 3000);
    CHECK_INT(flags, DX_UI_DIRTY);
    /* A clock change alone also redraws. */
    flags = 0;
    fake.clock_valid = 1;
    CHECK_INT(dx_screen_stat.tick(NULL, 1999, &flags), 2000);
    CHECK_INT(flags, DX_UI_DIRTY);
    fake.clock_valid = 0;
    /* Through the shell: one frame per second, none in between. */
    fake.presents = 0;
    fake.ticks = 2000;
    fake_clear_draws();
    dx_app_step(&app);
    CHECK_INT(fake.presents, 1);
    CHECK(fake_find_text("0d 00:00:02") != NULL);
    CHECK_INT(fake.last_timeout, 1000);
    fake.ticks = 2500;
    dx_app_step(&app);
    CHECK_INT(fake.presents, 1);
    CHECK_INT(fake.last_timeout, 500);
    fake.ticks = 3000;
    dx_app_step(&app);
    CHECK_INT(fake.presents, 2);
    /* Another sub-tab stops the tick. */
    key(DX_UI_LEFT);
    CHECK_INT(app.wake_on, 0);
    CHECK_INT(fake.last_timeout, DX_WAIT_FOREVER);
    /* Back on ABOUT it starts again; another tab stops it. */
    key(DX_UI_RIGHT);
    CHECK_INT(app.wake_on, 1);
    key(DX_UI_DIAL);
    CHECK_INT(app.tab, 1);
    CHECK_INT(app.wake_on, 0);
}

TEST(about_day_and_tick_wrap)
{
    unsigned flags = 0;

    boot(FULL);
    fake.ticks = 86399500u;
    key(DX_UI_RIGHT);
    key(DX_UI_RIGHT);
    CHECK(fake_find_text("0d 23:59:59") != NULL);
    CHECK_INT(app.wake, 86400000u);
    fake.ticks = 86400000u;
    fake_clear_draws();
    dx_app_step(&app);
    CHECK(fake_find_text("1d 00:00:00") != NULL);
    /* The tick counter wraps at 2^31: Pip-OS has nothing else, so the
     * uptime shown starts again from 0d 00:00:00 after 24.8 days. */
    /* Days later: a new visit to ABOUT, just before the wrap. */
    fake.ticks = 0x7fffffffu - 100;
    key(DX_UI_LEFT);
    fake_clear_draws();
    key(DX_UI_RIGHT);
    CHECK(fake_find_text("24d 20:31:23") != NULL);
    CHECK_INT(app.wake, dx_ticks_add(0x7fffffffu - 100, 1000 - (0x7fffffffu - 100) % 1000));
    CHECK_INT(app.wake, 352);
    CHECK_INT(dx_screen_stat.tick(NULL, 0x7fffffffu, &flags), 352);
    flags = 0;
    fake.ticks = 352;
    fake_clear_draws();
    dx_app_step(&app);
    CHECK(fake_find_text("0d 00:00:00") != NULL);
    CHECK_INT(app.wake, 1000);
}

/* Every view with the full profile stays inside the body. */
TEST(every_view_inside_body)
{
    int sub, i;

    boot(FULL);
    for (sub = 0; sub < 3; sub++) {
        for (i = 0; i < 12; i++) {
            draw_body();
            CHECK(body_inside());
            key(DX_UI_DOWN);
        }
        key(DX_UI_RIGHT);
    }
}

TEST(zz_no_leaks)
{
    fake_reset();
    CHECK_INT(fake_live_blocks(), 0);
}
