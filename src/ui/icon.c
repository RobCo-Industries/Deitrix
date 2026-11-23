#include "ui/icon.h"

const dx_icon_t *dx_icon_find(const char *name)
{
    int i;

    for (i = 0; i < dx_icon_count; i++) {
        const char *a = dx_icons[i].name;
        const char *b = name;

        while (*a != '\0' && *a == *b) {
            a++;
            b++;
        }
        if (*a == *b) {
            return &dx_icons[i];
        }
    }
    return NULL;
}

void dx_icon_draw(const dx_icon_t *icon, const dx_ctx_t *ctx, dx_rect_t area, uint32_t color)
{
    if (icon == NULL || icon->w > area.w || icon->h > area.h) {
        return;
    }
    ctx->gfx->blit1(area.x + (area.w - icon->w) / 2, area.y + (area.h - icon->h) / 2, icon->w,
                    icon->h, icon->bits, icon->stride, color, DX_COLOR_NONE);
}
