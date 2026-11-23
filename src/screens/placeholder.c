#include "screens/screens.h"

#include "ui/frame.h"

#define NAME_SCALE 3
#define NAME_DY    50  /* from the body top: y 72 on screen */
#define NOTE_DY    122 /* y 144 */

void dx_placeholder_draw(const dx_ctx_t *ctx, dx_rect_t area, const char *name)
{
    dx_text_centered(ctx, area.y + NAME_DY, name, ctx->theme.fg, NAME_SCALE);
    dx_text_centered(ctx, area.y + NOTE_DY, "Not built yet.", ctx->theme.dim, 1);
}
