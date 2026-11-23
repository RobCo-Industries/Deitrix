/* The RADIO tab: the radio screen. A placeholder for now, with SELECT to cycle the stations. */
#include "screens/screens.h"

#include "content.h"

#include "services/services.h"

/* SELECT cycles: off, the first station, the second, and so on, then
 * off again. It logs "deitrix: radio station=<N>" or "deitrix: radio off". */
static unsigned event(void *st, const dx_ui_event_t *ev)
{
    const dx_radio_t *r = dx_content_radio();
    int next;
    char line[40];
    dx_sb_t sb;

    (void)st;
    if (ev->type != DX_UI_SELECT || ev->repeat || r->count == 0) {
        return DX_UI_IGNORED;
    }
    next = dx_radio_current() + 1;
    if (dx_radio_state() == DX_RADIO_OFF) {
        /* After a failed start or the end of a track, go on from the last one. */
        int last = dx_radio_find(r, dx_content_settings()->last_station);

        next = dx_content_settings()->last_station == 0 ? 0 : last + 1;
    }
    /* The line goes out before the sound starts, so a test sees the
     * audio trace after it. */
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: radio ");
    if (next >= r->count) {
        dx_sb_str(&sb, "off\n");
        dx_plat_log(line);
        dx_radio_stop();
        dx_settings_set_station(dx_content_settings(), 0);
    } else {
        dx_sb_str(&sb, "station=");
        dx_sb_uint(&sb, (uint32_t)r->stations[next].number);
        dx_sb_char(&sb, '\n');
        dx_plat_log(line);
        dx_settings_set_station(dx_content_settings(), r->stations[next].number);
        if (dx_radio_play(next, 0) != 0) {
            dx_plat_log("deitrix: radio cannot play\n");
        }
    }
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

/* The station the last frame showed, -1 for off. A frame that shows
 * another logs "deitrix: radio shows <line>", so a target test can read
 * what the tab shows. The line goes out just before the present. */
static int shown = -1;

static void log_shown(int cur, const char *text)
{
    char line[64]; /* the prefix, "Playing ", a name of 20, "\n" */
    dx_sb_t sb;

    if (cur == shown) {
        return;
    }
    shown = cur;
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: radio shows ");
    dx_sb_str(&sb, text);
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
}

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_radio_t *r = dx_content_radio();
    char a[40];
    char b[40];
    dx_sb_t sb;
    int cur = dx_radio_current();

    (void)st;
    dx_placeholder_draw(ctx, area, "RADIO");
    if (r->load.status != DX_LOAD_OK) {
        dx_placeholder_lines(ctx, area, dx_placeholder_reason(&r->load, "RADIO.INI", a, sizeof(a)),
                             NULL);
        return;
    }
    dx_sb_init(&sb, a, sizeof(a));
    dx_sb_uint(&sb, (uint32_t)r->count);
    dx_sb_str(&sb, r->count == 1 ? " station" : " stations");
    dx_sb_init(&sb, b, sizeof(b));
    if (cur >= 0) {
        dx_sb_str(&sb, "Playing ");
        dx_sb_str(&sb, r->stations[cur].name);
    } else {
        dx_sb_str(&sb, "Off");
    }
    dx_placeholder_lines(ctx, area, a, b);
    log_shown(cur, b);
}

const dx_screen_t dx_screen_radio = {
    "RADIO", NULL, NULL, event, NULL, draw, NULL, NULL,
};
