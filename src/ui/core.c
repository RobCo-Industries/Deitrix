#include "ui/ui.h"

#include "model/util.h"
#include "services/services.h"

dx_theme_t dx_theme_from_fg(uint32_t fg)
{
    dx_theme_t t;

    t.fg = fg & 0xffffffu;
    t.dim = (t.fg >> 1) & 0x7f7f7fu;
    t.faint = (t.fg >> 2) & 0x3f3f3fu;
    t.bg = 0;
    return t;
}

uint32_t dx_ticks_since(uint32_t a, uint32_t b)
{
    return (b - a) & DX_TICKS_MASK;
}

uint32_t dx_ticks_add(uint32_t t, uint32_t ms)
{
    uint32_t r;

    if (ms > DX_TICKS_MAX_MS) {
        ms = DX_TICKS_MAX_MS;
    }
    r = (t + ms) & DX_TICKS_MASK;
    return r != 0 ? r : 1;
}

int dx_ticks_due(uint32_t now, uint32_t wake)
{
    return dx_ticks_since(wake, now) <= (DX_TICKS_MASK >> 1);
}

int dx_ui_translate(const dx_event_t *ev, dx_ui_event_t *out)
{
    static const int buttons[] = {
        DX_UI_UP, DX_UI_DOWN, DX_UI_LEFT, DX_UI_RIGHT, DX_UI_SELECT, DX_UI_SELECT, DX_UI_BACK,
    };

    out->repeat = 0;
    out->delta = 0;
    if (ev->type == DX_EV_DIAL) {
        if (ev->value == 0) {
            return 0;
        }
        out->type = DX_UI_DIAL;
        out->delta = ev->value;
        return 1;
    }
    if (ev->type != DX_EV_BUTTON || ev->code > DX_BTN_BACK) {
        return 0;
    }
    if (ev->value != DX_PRESS_DOWN && ev->value != DX_PRESS_REPEAT) {
        return 0;
    }
    out->type = buttons[ev->code];
    out->repeat = ev->value == DX_PRESS_REPEAT;
    return 1;
}

int dx_ui_is_back(const dx_ui_event_t *ev)
{
    return ev->type == DX_UI_BACK || ev->type == DX_UI_LEFT;
}

int dx_ui_sound_for(const dx_ui_event_t *ev, unsigned result, int tab_changed)
{
    if (tab_changed) {
        return DX_SND_MOVE;
    }
    if (result & DX_UI_HANDLED) {
        if (ev->type == DX_UI_SELECT) {
            return DX_SND_SELECT;
        }
        if (ev->type == DX_UI_BACK) {
            return DX_SND_BACK;
        }
        return (result & DX_UI_DIRTY) ? DX_SND_MOVE : DX_SND_NONE;
    }
    if (ev->repeat) {
        return DX_SND_NONE;
    }
    switch (ev->type) {
    case DX_UI_UP:
    case DX_UI_DOWN:
    case DX_UI_SELECT:
    case DX_UI_BACK:
        return DX_SND_DENY;
    default:
        return DX_SND_NONE;
    }
}

/* The owner's setting, and the radio's claim on the one voice. */
static int sound_on = 1;
static int sound_full;
static int sound_hum;
static int sound_blocked;

void dx_ui_sound_enable(int level)
{
    if (sound_hum && level != DX_UI_SOUND_FULL_HUM) {
        char line[48];
        dx_sb_t sb;

        /* The hum stops now, not at its next top-up. The line gives the
         * change's time, for the tests. */
        dx_sb_init(&sb, line, sizeof(line));
        dx_sb_str(&sb, "deitrix: sound left hum t_ms=");
        dx_sb_int(&sb, (int32_t)dx_plat_ticks());
        dx_sb_str(&sb, "\n");
        dx_plat_log(line);
        dx_hum_setting_left();
    }
    sound_on = level != DX_UI_SOUND_OFF;
    sound_full = level == DX_UI_SOUND_FULL || level == DX_UI_SOUND_FULL_HUM;
    sound_hum = level == DX_UI_SOUND_FULL_HUM;
}

int dx_ui_sound_hum(void)
{
    return DX_UI_SOUND && sound_hum;
}

int dx_ui_sound_enabled(void)
{
    return DX_UI_SOUND && sound_on;
}

void dx_ui_sound_block(int blocked)
{
    sound_blocked = blocked != 0;
}

void dx_ui_sound(int snd)
{
    /* Hz and ms per DX_SND_*. */
    static const uint16_t tones[][2] = {
        { 0, 0 }, { 1400, 12 }, { 1900, 30 }, { 900, 30 }, { 180, 70 },
    };

    if (!DX_UI_SOUND || !sound_on || sound_blocked || snd <= DX_SND_NONE || snd > DX_SND_DENY) {
        return;
    }
    dx_plat_tone(tones[snd][0], tones[snd][1]);
    dx_hum_halt(); /* the tone took the voice */
}

int dx_ui_sound_table(int snd, unsigned result, int tab_changed)
{
    /* One event, one sound: mode, then tab, then row, then the tone. */
    switch (snd) {
    case DX_SND_MOVE:
        if (tab_changed) {
            return DX_SFX_ID_MODE;
        }
        if (result & DX_UI_TAB_MOVED) {
            return DX_SFX_ID_TAB;
        }
        if (result & DX_UI_ROW_MOVED) {
            return DX_SFX_ID_SCROLL;
        }
        return -1;
    case DX_SND_SELECT:
        return DX_SFX_ID_SELECT;
    case DX_SND_BACK:
        return DX_SFX_ID_HIGHLIGHT;
    default:
        return -1;
    }
}

int dx_ui_sound_play(int id)
{
    return DX_UI_SOUND && sound_full && !sound_blocked && dx_sfx_play(id);
}

void dx_ui_sound_event(int snd, unsigned result, int tab_changed)
{
    if (result & DX_UI_SOUNDED) {
        return; /* the screen played the event's own sound */
    }
    if (DX_UI_SOUND && sound_full && !sound_blocked) {
        int id = dx_ui_sound_table(snd, result, tab_changed);

        if (id >= 0 && dx_sfx_play(id)) {
            return;
        }
    }
    dx_ui_sound(snd);
}

int dx_ui_sound_next(int level, int full)
{
    if (!full) {
        return level == DX_UI_SOUND_OFF ? DX_UI_SOUND_ON : DX_UI_SOUND_OFF;
    }
    return level >= DX_UI_SOUND_FULL_HUM ? DX_UI_SOUND_OFF : level + 1;
}

const char *dx_ui_sound_label(int level, int full)
{
    if (level == DX_UI_SOUND_OFF) {
        return "Off";
    }
    if (!full || level < DX_UI_SOUND_FULL) {
        return "On";
    }
    return level == DX_UI_SOUND_FULL_HUM ? "Full + hum" : "Full";
}

int dx_strlen(const char *s)
{
    int n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

int dx_text_width(const char *text, int scale)
{
    return dx_strlen(text) * DX_FONT_W * scale;
}

#define CUT_MAX 41 /* 40 columns and the NUL */

int dx_text_cut(const dx_ctx_t *ctx, int x, int y, const char *s, int cols, uint32_t fg,
                uint32_t bg)
{
    char buf[CUT_MAX];
    int len = dx_strlen(s);
    int n, i;

    if (cols > CUT_MAX - 1) {
        cols = CUT_MAX - 1;
    }
    if (cols <= 0 || len == 0) {
        return 0;
    }
    n = len <= cols ? len : cols - 1;
    for (i = 0; i < n; i++) {
        buf[i] = s[i] == '\t' ? ' ' : s[i];
    }
    if (len > cols) {
        buf[n++] = '~';
    }
    buf[n] = '\0';
    ctx->gfx->text(x, y, buf, fg, bg, 1);
    return n;
}

int dx_itoa(int32_t v, char *buf)
{
    char tmp[11];
    uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
    int n = 0, i = 0;

    do {
        tmp[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u != 0);
    if (v < 0) {
        buf[i++] = '-';
    }
    while (n > 0) {
        buf[i++] = tmp[--n];
    }
    buf[i] = '\0';
    return i;
}

void dx_scrollbar(const dx_ctx_t *ctx, dx_rect_t r, int first, int visible, int total)
{
    int thumb, travel, y;

    if (total <= visible || visible <= 0 || r.h <= 0) {
        return;
    }
    if (first < 0) {
        first = 0;
    }
    if (first > total - visible) {
        first = total - visible;
    }
    thumb = r.h * visible / total;
    if (thumb < 4) {
        thumb = 4;
    }
    travel = r.h - thumb;
    y = r.y + travel * first / (total - visible);
    ctx->gfx->line(r.x + r.w / 2, r.y, r.x + r.w / 2, r.y + r.h - 1, ctx->theme.faint);
    ctx->gfx->rect(r.x, y, r.w, thumb, ctx->theme.fg, 1);
}
