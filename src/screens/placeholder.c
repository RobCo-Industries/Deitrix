#include "screens/placeholder.h"

#include "model/nav.h"
#include "platform/platform.h"

#define NAME_SCALE 3
#define NAME_Y     72
#define NOTE_Y     144

void dx_placeholder_draw(const dx_theme_t *theme, int tab)
{
    dx_plat_color(theme->fg, DX_COLOR_NONE);
    dx_text_centered(NAME_Y, dx_tab_name(tab), NAME_SCALE);
    dx_plat_color(theme->dim, DX_COLOR_NONE);
    dx_text_centered(NOTE_Y, "Not built yet.", 1);
}
