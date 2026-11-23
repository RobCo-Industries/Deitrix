/* The MAP tab through the app shell: the views, the keys, the hints,
 * the empty states and the limits of MAP.INI. */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "content.h"
#include "fake_platform.h"
#include "model/map_view.h"
#include "screens/map_screen.h"
#include "screens/screens.h"
#include "ui/frame.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))
#define MAP_PATH "/PIPBOY/DATA/MAP.INI"
#define DENY_HZ  180
#define TAB_MAP  3

static const char SAMPLE[] =
    "version = 1\n"
    "[map.local]\ntitle = Alder Brook\nbox = 0,0,400,300\n"
    "[map.local.lines]\n"
    "l1 = 0,40 60,60 120,100 170,150 200,210 260,250 330,270 400,280\n"
    "l2 = 260,130 300,130 300,160 260,160 260,130\n"
    "[map.local.places]\n"
    "p1 = 280,145,home,House\n"
    "p2 = 70,62,water,Spring\n"
    "p3 = 150,120,hazard,Steep bank\n"
    "[map.world]\ntitle = The valley\nbox = 0,0,800,600\n"
    "[map.world.lines]\nl1 = 0,80 150,120 260,220\n"
    "[map.world.places]\np1 = 330,330,home,Alder Brook\np2 = 660,530,water,Long pond\n";

static const char LOCAL_ONLY[] =
    "version = 1\n[map.local]\nbox = 0,0,10,10\n"
    "[map.local.places]\np1 = 5,5,marker,Only one\n";

static const char NO_PLACES[] =
    "version = 1\n[map.world]\ntitle = Bare\nbox = 0,0,10,10\n"
    "[map.world.lines]\nl1 = 0,0 10,10\n";

static dx_app_t app;
static char big[40000];

static void boot(const char *map)
{
    fake_reset();
    if (map != NULL) {
        fake_add_text(MAP_PATH, map);
    }
    dx_content_load();
    dx_map_reset();
    CHECK_INT(dx_app_init(&app, dx_rom_screens, dx_rom_screen_count), 0);
    dx_app_step(&app);
    fake.log[0] = '\0';
}

/* Routes one key, then runs a step so the shell redraws. The draw
 * list starts empty: a full frame of the largest map nearly fills it. */
static void key(int type)
{
    dx_ui_event_t ev = { type, 0, 0 };

    fake_clear_draws();

    if (type == DX_UI_DIAL) {
        ev.delta = 1;
    }
    dx_app_route(&app, &ev);
    dx_app_step(&app);
}

static void dial(int32_t delta)
{
    dx_ui_event_t ev = { DX_UI_DIAL, 0, delta };

    fake_clear_draws();

    dx_app_route(&app, &ev);
    dx_app_step(&app);
}

/* STAT to MAP, the log cleared. */
static void to_map(void)
{
    dial(TAB_MAP);
    CHECK_INT(app.tab, TAB_MAP);
}

static void draw_body(void)
{
    dx_ctx_t c = fake_ctx();

    fake_clear_draws();
    dx_screen_map.draw(NULL, &c, dx_frame_body());
}

static const char *hint(void)
{
    const char *h = dx_screen_map.hint(NULL);

    return h != NULL ? h : DX_FRAME_HINT;
}

/* The last "map shows" or "map detail" line of the log, or "". */
static const char *last_view(void)
{
    static char out[96];
    const char *p = fake.log;
    const char *at = NULL;

    while ((p = strstr(p, "deitrix: map ")) != NULL) {
        if (strncmp(p, "deitrix: map drew", 17) != 0) {
            at = p;
        }
        p++;
    }
    out[0] = '\0';
    if (at != NULL) {
        size_t n = strcspn(at, "\n");

        memcpy(out, at, n);
        out[n] = '\0';
    }
    return out;
}

TEST(first_view)
{
    const fake_text_t *t;

    boot(SAMPLE);
    to_map();
    /* The first frame after the tab opens logs nothing of its own. */
    CHECK_STR(fake.log, "deitrix: tab=MAP\n");
    draw_body();
    /* The sub-tabs, the title, the selected place and its kind. */
    CHECK(fake_find_text("LOCAL") != NULL && fake_find_text("WORLD") != NULL);
    t = fake_find_text("Alder Brook");
    CHECK(t != NULL && t->fg == fake.theme.dim && t->x + 11 * 8 == 316);
    t = fake_find_text("House");
    CHECK(t != NULL && t->fg == fake.theme.fg && t->x == 4 && t->y == 197);
    t = fake_find_text("home");
    CHECK(t != NULL && t->fg == fake.theme.dim && t->x + 4 * 8 == 316 && t->y == 197);
    /* The lines dim, under the clip of the pane, and the clip off. */
    CHECK(strstr(fake.draws, "clip 0 48 320 145\npolyline 8 open dim\npolyline 5 open dim\n") !=
          NULL);
    CHECK(strstr(fake.draws, "clip off\n") != NULL);
    /* The rule over the place row. */
    CHECK(strstr(fake.draws, "line 4 193 315 193 faint\n") != NULL);
    CHECK(fake_all_points_inside(dx_frame_body()));
    CHECK_STR(hint(), "UP/DOWN: place  SELECT: open");
    /* Later frames log the view and the time the map took. */
    fake.log[0] = '\0';
    key(DX_UI_DOWN);
    CHECK_STR(fake.log, "deitrix: map shows LOCAL place=1\ndeitrix: map drew 0 ms\n");
}

TEST(markers)
{
    dx_map_fit_t f;
    char want[64];
    int px, py;
    dx_map_square_t s;

    boot(SAMPLE);
    to_map();
    draw_body();
    f = dx_map_pane_fit(&dx_content_maps()->maps[0], 0, 48, 320, 145);
    /* An unselected place: a 5 px square in fg. */
    dx_map_to_px(&f, 70, 62, &px, &py);
    s = dx_map_square(px, py, DX_MAP_MARK);
    tfmt(want, sizeof(want), "rect %d %d 5 5 fg fill\n", s.x, s.y);
    CHECK(strstr(fake.draws, want) != NULL);
    /* The selected one: the reticle, drawn last over a bg square. */
    dx_map_to_px(&f, 280, 145, &px, &py);
    s = dx_map_square(px, py, DX_MAP_MARK_SEL);
    tfmt(want, sizeof(want), "rect %d %d 9 9 bg fill\nrect %d %d 9 9 fg outline\n", s.x, s.y,
         s.x, s.y);
    CHECK(strstr(fake.draws, want) != NULL);
    tfmt(want, sizeof(want), "line %d %d %d %d fg\n", px, py - DX_MAP_REACH, px, s.y - 1);
    CHECK(strstr(fake.draws, want) != NULL);
}

TEST(cycle_both_ways_with_wrap)
{
    boot(SAMPLE);
    to_map();
    key(DX_UI_DOWN);
    CHECK_STR(last_view(), "deitrix: map shows LOCAL place=1");
    key(DX_UI_DOWN);
    CHECK_STR(last_view(), "deitrix: map shows LOCAL place=2");
    key(DX_UI_DOWN);
    CHECK_STR(last_view(), "deitrix: map shows LOCAL place=0");
    key(DX_UI_UP);
    CHECK_STR(last_view(), "deitrix: map shows LOCAL place=2");
    draw_body();
    CHECK(fake_find_text("Steep bank") != NULL && fake_find_text("hazard") != NULL);
}

TEST(detail_and_back)
{
    const fake_text_t *t;
    int i;
    static const int backs[2] = { DX_UI_BACK, DX_UI_LEFT };

    for (i = 0; i < 2; i++) {
        boot(SAMPLE);
        to_map();
        key(DX_UI_DOWN);
        key(DX_UI_SELECT);
        CHECK_STR(last_view(), "deitrix: map detail LOCAL place=1");
        CHECK_STR(hint(), "BACK: map");
        draw_body();
        t = fake_find_text("Spring");
        CHECK(t != NULL && t->fg == fake.theme.fg && t->y == 23);
        CHECK(fake_find_text("water") != NULL);
        CHECK(fake_find_text("70, 62") != NULL);
        CHECK(fake_find_text("Alder Brook") != NULL);
        CHECK(fake_find_text("2 of 3") != NULL);
        CHECK(fake_all_points_inside(dx_frame_body()));
        /* RIGHT, UP, DOWN and SELECT stay in the detail; only RIGHT is
         * handled, the rest deny. */
        fake.tone_count = 0;
        fake.log[0] = '\0';
        key(DX_UI_RIGHT);
        key(DX_UI_UP);
        key(DX_UI_DOWN);
        key(DX_UI_SELECT);
        CHECK_INT(app.tab, TAB_MAP);
        CHECK_STR(fake.log, "");
        CHECK_INT(fake.tone_count, 3);
        CHECK_INT(fake.tones[0][0], DENY_HZ);
        /* Back with the place kept. */
        key(backs[i]);
        CHECK_INT(app.tab, TAB_MAP);
        CHECK_STR(last_view(), "deitrix: map shows LOCAL place=1");
    }
}

TEST(sub_tabs_and_neighbours)
{
    boot(SAMPLE);
    to_map();
    key(DX_UI_DOWN);
    key(DX_UI_RIGHT);
    CHECK_INT(app.tab, TAB_MAP);
    CHECK_STR(last_view(), "deitrix: map shows WORLD place=0");
    CHECK_STR(hint(), "UP/DOWN: place  SELECT: open");
    key(DX_UI_DOWN);
    /* RIGHT on the last sub-tab: RADIO. Back by the dial: this card has
     * no stations, so RADIO's cursor is on Volume and takes LEFT.
     * Back on MAP: WORLD and its place. */
    key(DX_UI_RIGHT);
    CHECK_INT(app.tab, TAB_MAP + 1);
    fake.log[0] = '\0';
    dial(-1);
    CHECK_INT(app.tab, TAB_MAP);
    CHECK_STR(last_view(), "");
    key(DX_UI_DOWN);
    CHECK_STR(last_view(), "deitrix: map shows WORLD place=0");
    key(DX_UI_UP);
    CHECK_STR(last_view(), "deitrix: map shows WORLD place=1");
    /* LEFT to LOCAL, its place kept, then LEFT to DATA. */
    key(DX_UI_LEFT);
    CHECK_STR(last_view(), "deitrix: map shows LOCAL place=1");
    key(DX_UI_LEFT);
    CHECK_INT(app.tab, TAB_MAP - 1);
}

TEST(dial_leaves_the_detail)
{
    boot(SAMPLE);
    to_map();
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    CHECK_STR(last_view(), "deitrix: map detail LOCAL place=1");
    dial(1);
    CHECK_INT(app.tab, TAB_MAP + 1);
    fake.log[0] = '\0';
    dial(-1);
    CHECK_INT(app.tab, TAB_MAP);
    CHECK_STR(last_view(), "");
    CHECK_STR(hint(), "UP/DOWN: place  SELECT: open");
    key(DX_UI_DOWN);
    CHECK_STR(last_view(), "deitrix: map shows LOCAL place=2");
}

TEST(one_map_one_place)
{
    boot(LOCAL_ONLY);
    to_map();
    CHECK_STR(hint(), "SELECT: open");
    draw_body();
    CHECK(fake_find_text("local") != NULL); /* the default title */
    CHECK(fake_find_text("WORLD") == NULL);
    /* One place: UP and DOWN deny. LEFT and RIGHT change the tab. */
    fake.tone_count = 0;
    key(DX_UI_UP);
    key(DX_UI_DOWN);
    CHECK_INT(fake.tone_count, 2);
    CHECK_INT(fake.tones[1][0], DENY_HZ);
    key(DX_UI_RIGHT);
    CHECK_INT(app.tab, TAB_MAP + 1);
}

TEST(no_places)
{
    const fake_text_t *t;

    boot(NO_PLACES);
    to_map();
    CHECK_STR(hint(), DX_FRAME_HINT);
    draw_body();
    t = fake_find_text("No places on this map.");
    CHECK(t != NULL && t->fg == fake.theme.dim);
    CHECK(strstr(fake.draws, "polyline 2 open dim\n") != NULL);
    fake.tone_count = 0;
    fake.log[0] = '\0';
    key(DX_UI_UP);
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    CHECK_INT(fake.tone_count, 3);
    CHECK_INT(fake.tones[0][0], DENY_HZ);
    CHECK_STR(last_view(), "");
}

TEST(places_without_lines)
{
    boot(LOCAL_ONLY);
    to_map();
    draw_body();
    CHECK(strstr(fake.draws, "polyline") == NULL);
    CHECK(strstr(fake.draws, "9 9 fg outline\n") != NULL);
}

static void empty_case(const char *file, const char *reason)
{
    const fake_text_t *t;

    boot(file);
    to_map();
    CHECK(dx_screen_map.hint(NULL) == NULL);
    draw_body();
    t = fake_find_text("No map on the card.");
    CHECK(t != NULL && t->fg == fake.theme.fg);
    t = fake_find_text(reason);
    CHECK(t != NULL && t->fg == fake.theme.dim);
    CHECK(fake_all_points_inside(dx_frame_body()));
    fake.tone_count = 0;
    key(DX_UI_UP);
    key(DX_UI_DOWN);
    key(DX_UI_SELECT);
    key(DX_UI_BACK);
    CHECK_INT(fake.tone_count, 4);
    CHECK_INT(app.tab, TAB_MAP);
    key(DX_UI_RIGHT);
    CHECK_INT(app.tab, TAB_MAP + 1);
}

TEST(no_file)
{
    empty_case(NULL, "MAP.INI missing");
}

TEST(rejected_file)
{
    empty_case("version = 2\n[map.local]\nbox = 0,0,9,9\n", "MAP.INI bad version");
}

TEST(file_without_maps)
{
    empty_case("version = 1\n[map.local]\nbox = 9,9,0,0\n", "MAP.INI holds no map.");
}

/* MAP.INI at the format's limits: both maps, 64 lines of 16 points at
 * the ends of the range, 24 places with 24-character names. */
static const char *max_map(void)
{
    size_t n = 0;
    int m, l, i;

    n += (size_t)tfmt(big + n, sizeof(big) - n, "version = 1\n");
    for (m = 0; m < 2; m++) {
        const char *name = m == 0 ? "local" : "world";

        n += (size_t)tfmt(big + n, sizeof(big) - n,
                          "[map.%s]\ntitle = ABCDEFGHIJKLMNOPQRSTUVWX\nbox = 0,0,999,999\n"
                          "[map.%s.lines]\n",
                          name, name);
        for (l = 1; l <= DX_LINES_MAX; l++) {
            n += (size_t)tfmt(big + n, sizeof(big) - n, "l%d =", l);
            for (i = 0; i < DX_POINTS_MAX; i++) {
                n += (size_t)tfmt(big + n, sizeof(big) - n, " %d,%d",
                                  (i + l) % 2 ? 999 : 0, (i / 2 + l) % 2 ? 999 : 0);
            }
            n += (size_t)tfmt(big + n, sizeof(big) - n, "\n");
        }
        n += (size_t)tfmt(big + n, sizeof(big) - n, "[map.%s.places]\n", name);
        for (i = 1; i <= DX_PLACES_MAX; i++) {
            n += (size_t)tfmt(big + n, sizeof(big) - n, "p%d = %d,%d,shelter,Place %02d "
                              "WWWWWWWWWWWWWWW\n",
                              i, i % 2 ? 999 : 0, i % 3 ? 999 : 0, i);
        }
    }
    return big;
}

TEST(limits_inside_body)
{
    const dx_maps_t *mm;
    int sub, i;

    boot(max_map());
    mm = dx_content_maps();
    CHECK_INT(mm->count, 2);
    CHECK_INT(mm->maps[0].line_count, DX_LINES_MAX);
    CHECK_INT(mm->maps[0].place_count, DX_PLACES_MAX);
    CHECK_INT(mm->maps[1].line_count, DX_LINES_MAX);
    to_map();
    for (sub = 0; sub < 2; sub++) {
        for (i = 0; i < DX_PLACES_MAX + 1; i++) {
            draw_body();
            CHECK(fake.points_seen > FAKE_MAX_POINTS);
            CHECK(fake_all_points_inside(dx_frame_body()));
            if (i == 3) {
                key(DX_UI_SELECT);
                draw_body();
                CHECK(fake_all_points_inside(dx_frame_body()));
                CHECK(fake_find_text("Place 04 WWWWWWWWWWWWWWW") != NULL);
                key(DX_UI_BACK);
            }
            fake.log[0] = '\0';
            key(DX_UI_DOWN);
        }
        CHECK_STR(last_view(), sub == 0 ? "deitrix: map shows LOCAL place=1"
                                        : "deitrix: map shows WORLD place=1");
        CHECK(strlen(hint()) <= DX_HINT_MAX);
        key(DX_UI_RIGHT);
    }
}

TEST(hints_fit)
{
    static const char *const files[] = { SAMPLE, LOCAL_ONLY, NO_PLACES, NULL };
    int f;

    for (f = 0; f < 4; f++) {
        boot(files[f]);
        to_map();
        CHECK(strlen(hint()) <= DX_HINT_MAX);
        key(DX_UI_SELECT);
        CHECK(strlen(hint()) <= DX_HINT_MAX);
        key(DX_UI_BACK);
        key(DX_UI_RIGHT);
        CHECK(strlen(hint()) <= DX_HINT_MAX);
    }
}

TEST(no_tick)
{
    CHECK(dx_screen_map.tick == NULL);
}

TEST(zz_no_leaks)
{
    fake_reset();
    CHECK_INT(fake_live_blocks(), 0);
}

/* The held-key rule through the shell: a held SELECT opens the detail
 * once, a held LEFT or BACK closes it once, and the map and the tab
 * stay. */
TEST(held_keys_open_and_close_once)
{
    static const uint32_t backs[2] = { DX_BTN_LEFT, DX_BTN_BACK };
    int i, n;

    for (i = 0; i < 2; i++) {
        boot(SAMPLE);
        to_map();
        fake.log[0] = '\0';
        for (n = fake_hold(DX_BTN_SELECT, 12); n > 0; n--) {
            dx_app_step(&app);
        }
        CHECK_STR(last_view(), "deitrix: map detail LOCAL place=0");
        CHECK(strstr(fake.log, "detail LOCAL place=0") ==
              strstr(fake.log, "detail LOCAL"));
        for (n = fake_hold(backs[i], 12); n > 0; n--) {
            dx_app_step(&app);
        }
        CHECK_INT(app.tab, TAB_MAP);
        CHECK_STR(last_view(), "deitrix: map shows LOCAL place=0");
        /* The next fresh LEFT acts again: LOCAL is the first map, so
         * the shell changes the tab. */
        for (n = fake_hold(DX_BTN_LEFT, 0); n > 0; n--) {
            dx_app_step(&app);
        }
        CHECK_INT(app.tab, TAB_MAP - 1);
    }
}
