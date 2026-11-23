#include "screens/screens.h"

#include "model/load.h"
#include "ui/frame.h"

#define NAME_SCALE 3
#define NAME_DY    50  /* from the body top: y 72 on screen */
#define NOTE_DY    122 /* y 144 */
#define LINE_DY    146 /* y 168, then 184 */

void dx_placeholder_draw(const dx_ctx_t *ctx, dx_rect_t area, const char *name)
{
    dx_text_centered(ctx, area.y + NAME_DY, name, ctx->theme.fg, NAME_SCALE);
    dx_text_centered(ctx, area.y + NOTE_DY, "Not built yet.", ctx->theme.dim, 1);
}

void dx_placeholder_lines(const dx_ctx_t *ctx, dx_rect_t area, const char *a, const char *b)
{
    if (a != NULL) {
        dx_text_centered(ctx, area.y + LINE_DY, a, ctx->theme.fg, 1);
    }
    if (b != NULL) {
        dx_text_centered(ctx, area.y + LINE_DY + DX_ROW_H, b, ctx->theme.fg, 1);
    }
}

const char *dx_placeholder_reason(const struct dx_load *ld, const char *file, char *buf, int cap)
{
    dx_sb_t sb;

    dx_sb_init(&sb, buf, cap);
    dx_sb_str(&sb, file);
    dx_sb_char(&sb, ' ');
    switch (ld->status) {
    case DX_LOAD_MISSING:
        dx_sb_str(&sb, "missing");
        break;
    case DX_LOAD_TOO_LARGE:
        dx_sb_str(&sb, "too large");
        break;
    case DX_LOAD_TOO_SMALL:
        dx_sb_str(&sb, "too small");
        break;
    case DX_LOAD_VERSION:
        dx_sb_str(&sb, "bad version");
        break;
    case DX_LOAD_PARSE:
    case DX_LOAD_BROKEN:
        dx_sb_str(&sb, "line ");
        dx_sb_uint(&sb, ld->line);
        break;
    default:
        dx_sb_str(&sb, "read error");
        break;
    }
    return buf;
}
