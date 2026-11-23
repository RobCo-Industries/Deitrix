#include "fake_platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

fake_platform_t fake;

void fake_reset(void)
{
    memset(&fake, 0, sizeof(fake));
    fake.fg = 0x33e066u;
    fake.theme = dx_theme_from_fg(fake.fg);
    fake.ticks = 1000;
}

void fake_push(uint32_t type, uint32_t code, int32_t value)
{
    dx_event_t *ev = &fake.events[fake.event_count++];

    ev->type = type;
    ev->code = code;
    ev->value = value;
}

void fake_clear_draws(void)
{
    fake.text_count = 0;
    fake.rect_count = 0;
    fake.point_count = 0;
    fake.presents = 0;
    fake.clears = 0;
    fake.draws[0] = '\0';
}

const fake_text_t *fake_find_text(const char *text)
{
    int i;

    for (i = 0; i < fake.text_count; i++) {
        if (strcmp(fake.texts[i].text, text) == 0) {
            return &fake.texts[i];
        }
    }
    return NULL;
}

int fake_points_inside(dx_rect_t r)
{
    int i;

    for (i = 0; i < fake.point_count; i++) {
        const fake_point_t *p = &fake.points[i];

        if (p->x < r.x || p->y < r.y || p->x >= r.x + r.w || p->y >= r.y + r.h) {
            return 0;
        }
    }
    return 1;
}

static void point(int x, int y)
{
    if (fake.point_count < FAKE_MAX_POINTS) {
        fake.points[fake.point_count].x = x;
        fake.points[fake.point_count].y = y;
        fake.point_count++;
    }
}

/* A colour as its theme name when it has one. */
static const char *color_name(uint32_t c, char buf[12])
{
    if (c == DX_COLOR_NONE) {
        return "none";
    }
    if (c == fake.theme.fg) {
        return "fg";
    }
    if (c == fake.theme.dim) {
        return "dim";
    }
    if (c == fake.theme.faint) {
        return "faint";
    }
    if (c == fake.theme.bg) {
        return "bg";
    }
    snprintf(buf, 12, "#%06x", (unsigned)c);
    return buf;
}

static void emit(const char *fmt, ...)
{
    size_t n = strlen(fake.draws);
    va_list ap;

    if (n >= sizeof(fake.draws) - 1) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(fake.draws + n, sizeof(fake.draws) - n, fmt, ap);
    va_end(ap);
    n = strlen(fake.draws);
    if (n < sizeof(fake.draws) - 1) {
        fake.draws[n] = '\n';
        fake.draws[n + 1] = '\0';
    }
}

static void f_clear(uint32_t color)
{
    char b[12];

    fake.clears++;
    emit("clear %s", color_name(color, b));
}

static void f_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    char b[12];

    point(x0, y0);
    point(x1, y1);
    emit("line %d %d %d %d %s", x0, y0, x1, y1, color_name(color, b));
}

static void f_rect(int x, int y, int w, int h, uint32_t color, int fill)
{
    char b[12];

    if (fake.rect_count < FAKE_MAX_RECTS) {
        fake_rect_t *r = &fake.rects[fake.rect_count++];

        r->x = x;
        r->y = y;
        r->w = w;
        r->h = h;
        r->filled = fill;
        r->fg = color;
    }
    point(x, y);
    point(x + w - 1, y + h - 1);
    emit("rect %d %d %d %d %s %s", x, y, w, h, color_name(color, b), fill ? "fill" : "outline");
}

static void f_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale)
{
    char b1[12], b2[12];

    if (fake.text_count < FAKE_MAX_TEXTS) {
        fake_text_t *t = &fake.texts[fake.text_count++];

        t->x = x;
        t->y = y;
        t->scale = scale;
        t->fg = fg;
        t->bg = bg;
        snprintf(t->text, sizeof(t->text), "%s", s);
    }
    point(x, y);
    point(x + (int)strlen(s) * DX_FONT_W * scale - 1, y + DX_FONT_H * scale - 1);
    emit("text %d %d \"%s\" %s %s s%d", x, y, s, color_name(fg, b1), color_name(bg, b2), scale);
}

static void f_polyline(const int16_t *xy, int points, int closed, uint32_t color)
{
    char b[12];
    int i;

    for (i = 0; i < points; i++) {
        point(xy[2 * i], xy[2 * i + 1]);
    }
    emit("polyline %d %s %s", points, closed ? "closed" : "open", color_name(color, b));
}

static void f_arc(int cx, int cy, int r, int start_deg, int end_deg, uint32_t color, int fill)
{
    char b[12];

    point(cx - r, cy - r);
    point(cx + r, cy + r);
    emit("arc %d %d %d %d %d %s %s", cx, cy, r, start_deg, end_deg, color_name(color, b),
         fill ? "fill" : "outline");
}

static void f_blit1(int x, int y, int w, int h, const uint8_t *bits, int stride, uint32_t fg,
                    uint32_t bg)
{
    char b1[12], b2[12];

    (void)bits;
    point(x, y);
    point(x + w - 1, y + h - 1);
    emit("blit1 %d %d %d %d stride %d %s %s", x, y, w, h, stride, color_name(fg, b1),
         color_name(bg, b2));
}

static void f_clip(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) {
        emit("clip off");
    } else {
        emit("clip %d %d %d %d", x, y, w, h);
    }
}

static void f_present(void)
{
    fake.presents++;
    emit("present");
}

static const dx_gfx_t recorder = {
    f_clear, f_line, f_rect, f_text, f_polyline, f_arc, f_blit1, f_clip, f_present,
};

const dx_gfx_t *dx_plat_gfx(void)
{
    return &recorder;
}

dx_ctx_t fake_ctx(void)
{
    dx_ctx_t c;

    c.gfx = &recorder;
    c.theme = fake.theme;
    c.now_ms = fake.ticks;
    return c;
}

uint32_t dx_plat_fg(void)
{
    return fake.fg;
}

int dx_plat_time(uint32_t *unix_seconds)
{
    *unix_seconds = fake.clock;
    return fake.clock_valid;
}

uint32_t dx_plat_ticks(void)
{
    return fake.ticks;
}

int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms)
{
    fake.waits++;
    fake.last_timeout = timeout_ms;
    fake.ticks += fake.tick_step;
    if (fake.event_next >= fake.event_count) {
        return 0;
    }
    *ev = fake.events[fake.event_next++];
    return 1;
}

void dx_plat_log(const char *text)
{
    strncat(fake.log, text, sizeof(fake.log) - strlen(fake.log) - 1);
}

void dx_plat_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (fake.tone_count < FAKE_MAX_TONES) {
        fake.tones[fake.tone_count][0] = freq_hz;
        fake.tones[fake.tone_count][1] = duration_ms;
        fake.tone_count++;
    }
}
