#include "ui/frame.h"

#include "model/nav.h"
#include "platform/platform.h"

#define TAB_X0      4   /* left edge of the first tab */
#define TAB_PAD     8   /* space on each side of a tab label */
#define HEADER_Y    3   /* text row of the header */
#define HEADER_RULE 21
#define FOOTER_RULE 215
#define FOOTER_Y    220
#define CLOCK_X     (DX_SCREEN_W - 4 - 5 * DX_FONT_W)

static const char hint[] = "DIAL or LEFT/RIGHT: change tab";

dx_theme_t dx_theme_from_fg(uint32_t fg)
{
    dx_theme_t t;

    t.fg = fg & 0xffffffu;
    t.dim = (t.fg >> 1) & 0x7f7f7fu;
    return t;
}

int32_t dx_text_width(const char *text, uint32_t scale)
{
    int32_t n = 0;

    while (text[n] != '\0') {
        n++;
    }
    return n * DX_FONT_W * (int32_t)scale;
}

void dx_text_centered(int32_t y, const char *text, uint32_t scale)
{
    dx_plat_text((DX_SCREEN_W - dx_text_width(text, scale)) / 2, y, text, scale);
}

int32_t dx_frame_tab_x(int tab)
{
    int32_t x = TAB_X0;
    int i;

    for (i = 0; i < tab && i < DX_TAB_COUNT; i++) {
        x += dx_text_width(dx_tab_name(i), 1) + 2 * TAB_PAD;
    }
    return x + TAB_PAD;
}

void dx_frame_draw(const dx_theme_t *theme, int active_tab, const char *clock)
{
    int i;

    for (i = 0; i < DX_TAB_COUNT; i++) {
        const char *name = dx_tab_name(i);
        int32_t x = dx_frame_tab_x(i);

        if (i == active_tab) {
            /* The active tab: dark text on a filled block. */
            dx_plat_color(theme->fg, 0);
            dx_plat_rect(x - TAB_PAD, 0, (uint32_t)(dx_text_width(name, 1) + 2 * TAB_PAD),
                         HEADER_RULE, 1);
            dx_plat_color(0, DX_COLOR_NONE);
        } else {
            dx_plat_color(theme->dim, DX_COLOR_NONE);
        }
        dx_plat_text(x, HEADER_Y, name, 1);
    }
    dx_plat_color(theme->fg, DX_COLOR_NONE);
    dx_plat_text(CLOCK_X, HEADER_Y, clock, 1);
    dx_plat_line(0, HEADER_RULE, DX_SCREEN_W - 1, HEADER_RULE);
    dx_plat_line(0, FOOTER_RULE, DX_SCREEN_W - 1, FOOTER_RULE);
    dx_plat_color(theme->dim, DX_COLOR_NONE);
    dx_text_centered(FOOTER_Y, hint, 1);
}
