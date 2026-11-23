/*
 * Every footer hint of every state follows one style (ui/frame.h): a
 * long random walk through the shell on three cards reads the hint
 * after each key and checks it, then checks that the walk saw every
 * hint of the table in docs/ARCHITECTURE.md.
 */
#include "rc_test.h"
#include "tfmt.h"

#include <string.h>

#include "app.h"
#include "content.h"
#include "fake_platform.h"
#include "screens/data_view.h"
#include "screens/inv_screen.h"
#include "screens/map_screen.h"
#include "screens/radio_screen.h"
#include "screens/screens.h"
#include "screens/stat_draw.h"
#include "services/services.h"
#include "ui/frame.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

/* The hints of docs/ARCHITECTURE.md's table, the shell's own last. */
static const char *const table[] = {
    "LEFT/RIGHT: page",
    "UP/DOWN: move",
    "UP/DOWN: move  SELECT: open",
    "UP/DOWN: +/-  SELECT: mark  BACK: list",
    "UP/DOWN: scroll  BACK: list",
    "BACK: list",
    "UP/DOWN: move  SELECT: run tape",
    "SELECT: set time",
    "UP/DOWN: set  SELECT: ok  BACK: cancel",
    "UP/DOWN: place  SELECT: open",
    "SELECT: open",
    "BACK: map",
    "UP/DOWN: move  SELECT: play",
    "UP/DOWN: move  SELECT: stop",
    "UP/DOWN: move  LEFT/RIGHT: volume",
    "UP/DOWN: move  SELECT: sound on/off",
    DX_FRAME_HINT,
};
#define TABLE_N ((int)(sizeof(table) / sizeof(table[0])))

static int seen[TABLE_N];
static char unknown[64];

/* The keys in their order: movement, then SELECT, then BACK, then the
 * shell's DIAL. */
static const char *const keys[] = { "UP/DOWN", "LEFT/RIGHT", "SELECT", "BACK", "DIAL" };

static int action_ok(const char *a, size_t n)
{
    size_t i;

    if (n == 0 || a[0] == ' ' || a[n - 1] == ' ') {
        return 0;
    }
    for (i = 0; i < n; i++) {
        char c = a[i];

        if (!((c >= 'a' && c <= 'z') || c == ' ' || c == '/' || c == '+' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

/* 1 when h follows the style. A screen's own hint never names DIAL. */
static int style_ok(const char *h, int own)
{
    int last = -1;
    const char *p = h;

    if (h == NULL || strlen(h) == 0 || strlen(h) > DX_HINT_MAX) {
        return 0;
    }
    for (;;) {
        const char *end = strstr(p, "  ");
        size_t n = end != NULL ? (size_t)(end - p) : strlen(p);
        int k;
        int found = -1;

        for (k = 0; k < (int)(sizeof(keys) / sizeof(keys[0])); k++) {
            size_t kl = strlen(keys[k]);

            if (n > kl + 2 && strncmp(p, keys[k], kl) == 0 && p[kl] == ':' && p[kl + 1] == ' ') {
                found = k;
                if (!action_ok(p + kl + 2, n - kl - 2)) {
                    return 0;
                }
            }
        }
        if (found <= last || (own && found == 4)) {
            return 0;
        }
        last = found;
        if (end == NULL) {
            return 1;
        }
        p = end + 2;
        if (*p == ' ') {
            return 0; /* three spaces */
        }
    }
}

static int check_hint(const dx_app_t *app)
{
    const dx_screen_t *s = app->screens[app->tab];
    const char *own = s->hint != NULL ? s->hint(s->st) : NULL;
    const char *h = own != NULL ? own : DX_FRAME_HINT;
    int i;

    if (!style_ok(h, own != NULL)) {
        tfmt(unknown, sizeof(unknown), "%s", h);
        return 0;
    }
    for (i = 0; i < TABLE_N; i++) {
        if (strcmp(h, table[i]) == 0) {
            seen[i] = 1;
            return 1;
        }
    }
    tfmt(unknown, sizeof(unknown), "%s", h);
    return 0;
}

static void reset_screens(void)
{
    dx_stat_reset();
    dx_inv_screen_reset();
    dx_data_reset();
    dx_map_reset();
    dx_radio_screen_reset();
}

/* A fixed pseudo-random walk of `n` keys from the first tab. */
static int walk(uint32_t seed, int n)
{
    static const int ui[] = { DX_UI_UP, DX_UI_DOWN, DX_UI_LEFT, DX_UI_RIGHT,
                              DX_UI_SELECT, DX_UI_BACK, DX_UI_DIAL };
    dx_app_t app;
    int i;

    dx_content_load();
    dx_radio_attach(dx_content_radio());
    reset_screens();
    if (dx_app_init(&app, dx_rom_screens, dx_rom_screen_count) != 0) {
        return 0;
    }
    fake_clear_draws();
    dx_app_step(&app);
    if (!check_hint(&app)) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        dx_ui_event_t ev = { 0, 0, 0 };
        uint32_t r;

        seed = seed * 1103515245u + 12345u;
        r = (seed >> 16) % 16u;
        /* The dial seldom, so the walk goes deep into each tab. */
        ev.type = r < 15 ? ui[r % 6] : DX_UI_DIAL;
        ev.repeat = (seed >> 8) % 5u == 0 && ev.type != DX_UI_DIAL;
        ev.delta = ev.type == DX_UI_DIAL ? ((seed >> 4) % 2u ? 1 : -1) : 0;
        fake_clear_draws();
        dx_app_route(&app, &ev);
        dx_app_step(&app);
        if (!check_hint(&app)) {
            return 0;
        }
        fake.log[0] = '\0';
    }
    dx_radio_stop();
    return 1;
}

/* The fake's audio ends at once, so the walk seldom sees a station on
 * air: SELECT on the first one, then read the hint at once. */
static int radio_on_air(void)
{
    dx_app_t app;
    dx_ui_event_t ev = { DX_UI_SELECT, 0, 0 };
    int ok;

    dx_content_load();
    dx_radio_attach(dx_content_radio());
    reset_screens();
    if (dx_app_init(&app, dx_rom_screens, dx_rom_screen_count) != 0) {
        return 0;
    }
    dx_app_set_tab(&app, dx_rom_screen_count - 1);
    fake_clear_draws();
    dx_app_step(&app);
    dx_app_route(&app, &ev);
    ok = check_hint(&app);
    dx_radio_stop();
    return ok;
}

static const char ITEMS_EMPTY_CAT[] =
    "version = 1\n[categories]\ntools = TOOLS\nspare = SPARE\n"
    "[item.knife]\nname = Folding knife\ncat = tools\n";

static const char MAP_NO_PLACES[] =
    "version = 1\n[map.local]\nbox = 0,0,10,10\n[map.local.lines]\nl1 = 0,0 10,10\n"
    "[map.world]\nbox = 0,0,10,10\n[map.world.places]\np1 = 5,5,marker,Only one\n";

TEST(every_hint_follows_the_style)
{
    int i;

    memset(seen, 0, sizeof(seen));
    unknown[0] = '\0';
    /* The sample card, with a tape. */
    fake_reset();
    fake.clock_valid = 1;
    fake.clock = 3402208025u;
    CHECK(fake_load_tree("content", "") > 0);
    fake_add_holo("QUICK.HTP", DX_HOLO_GAME, "Quick", NULL);
    CHECK(walk(1u, 6000));
    CHECK_STR(unknown, "");
    CHECK(radio_on_air());
    CHECK_STR(unknown, "");
    /* An empty card. */
    fake_reset();
    CHECK(walk(2u, 2000));
    CHECK_STR(unknown, "");
    /* An empty category, a map without places, one short note. */
    fake_reset();
    fake_add_text("/PIPBOY/DATA/ITEMS.INI", ITEMS_EMPTY_CAT);
    fake_add_text("/PIPBOY/DATA/MAP.INI", MAP_NO_PLACES);
    fake_add_text("/PIPBOY/DATA/NOTES/A.TXT", "Alpha\nShort.\n");
    CHECK(walk(3u, 4000));
    CHECK_STR(unknown, "");
    for (i = 0; i < TABLE_N; i++) {
        if (!seen[i]) {
            tfmt(unknown, sizeof(unknown), "%s", table[i]);
            break;
        }
    }
    /* The walk reached every state of the table. */
    CHECK_STR(unknown, "");
}

TEST(the_rules_refuse_a_bad_hint)
{
    CHECK(style_ok("UP/DOWN: move  SELECT: open", 1));
    CHECK(!style_ok("UP/DOWN: move   SELECT: open", 1));    /* three spaces */
    CHECK(!style_ok("SELECT: open  UP/DOWN: move", 1));     /* order */
    CHECK(!style_ok("UP/DN: move", 1));                     /* key name */
    CHECK(!style_ok("UP/DOWN: move  DIAL: tab", 1));        /* a screen names the tab */
    CHECK(style_ok(DX_FRAME_HINT, 0));
    CHECK(!style_ok("UP/DOWN: place  SELECT: open  RIGHT: map", 1)); /* RIGHT alone */
    CHECK(!style_ok("UP/DOWN: move  SELECT: open  BACK: back to it", 1)); /* 39 */
    CHECK(!style_ok("BACK", 1));                            /* no action */
}

TEST(zz_no_leaks)
{
    fake_reset();
    CHECK_INT(fake_live_blocks(), 0);
}
