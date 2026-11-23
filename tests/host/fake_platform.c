#include "fake_platform.h"

#include <string.h>

fake_platform_t fake;

static uint32_t cur_fg, cur_bg;

void fake_reset(void)
{
    memset(&fake, 0, sizeof(fake));
    fake.fg = 0x33e066u;
    cur_fg = 0;
    cur_bg = 0;
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
    fake.presents = 0;
    fake.clears = 0;
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

uint32_t dx_plat_fg(void)
{
    return fake.fg;
}

int dx_plat_time(uint32_t *unix_seconds)
{
    *unix_seconds = fake.clock;
    return fake.clock_valid;
}

int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms)
{
    fake.waits++;
    fake.last_timeout = timeout_ms;
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

void dx_plat_clear(uint32_t color)
{
    (void)color;
    fake.clears++;
}

void dx_plat_color(uint32_t fg, uint32_t bg)
{
    cur_fg = fg;
    cur_bg = bg;
}

void dx_plat_text(int32_t x, int32_t y, const char *text, uint32_t scale)
{
    fake_text_t *t;

    if (fake.text_count >= FAKE_MAX_TEXTS) {
        return;
    }
    t = &fake.texts[fake.text_count++];
    t->x = x;
    t->y = y;
    t->scale = scale;
    t->fg = cur_fg;
    t->bg = cur_bg;
    strncpy(t->text, text, sizeof(t->text) - 1);
    t->text[sizeof(t->text) - 1] = '\0';
}

void dx_plat_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, int filled)
{
    fake_rect_t *r;

    if (fake.rect_count >= FAKE_MAX_RECTS) {
        return;
    }
    r = &fake.rects[fake.rect_count++];
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    r->filled = filled;
    r->fg = cur_fg;
}

void dx_plat_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
}

void dx_plat_present(void)
{
    fake.presents++;
}
