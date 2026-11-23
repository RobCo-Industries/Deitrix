#include "screens/stat_draw.h"

#include "ui/gauge.h"
#include "ui/vfont.h"

#define GAUGES_MAX 6

int dx_stat_name_unit(const char *name, int width)
{
    int unit;
    int i;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (i = 0; name[i] != '\0'; i++) {
        if (dx_vfont_glyph(name[i]) == NULL) {
            return 0;
        }
    }
    for (unit = DX_STAT_NAME_UNIT_MAX; unit >= DX_STAT_NAME_UNIT_MIN; unit--) {
        if (dx_vfont_width(name, unit) + dx_vfont_weight(unit) - 1 <= width) {
            return unit;
        }
    }
    return 0;
}

void dx_stat_draw_name(const dx_ctx_t *ctx, dx_rect_t area, int y, int h, const char *name)
{
    int unit = dx_stat_name_unit(name, area.w - 8);

    if (unit > 0) {
        int w = dx_vfont_width(name, unit) + dx_vfont_weight(unit) - 1;
        int gh = DX_VFONT_H * unit + dx_vfont_weight(unit) - 1;

        dx_vfont_draw(ctx, area.x + (area.w - w) / 2, y + (h - gh) / 2, name, unit,
                      ctx->theme.fg);
    } else {
        int cols = (area.w - 8) / DX_FONT_W;
        int n = dx_strlen(name);

        if (n > cols) {
            n = cols;
        }
        dx_text_cut(ctx, area.x + (area.w - n * DX_FONT_W) / 2, y + (h - DX_FONT_H) / 2, name,
                    cols, ctx->theme.fg, DX_COLOR_NONE);
    }
}

void dx_stat_draw_gauges(const dx_ctx_t *ctx, dx_rect_t area, int pitch,
                         const char *const *labels, const int32_t *values,
                         const int32_t *maxes, int n)
{
    dx_gauge_t gs[GAUGES_MAX] = { { NULL, 0, 0, 0, 0, 0 } };
    int label_cols = 0;
    int value_cols;
    int i;

    if (n > GAUGES_MAX) {
        n = GAUGES_MAX;
    }
    if (n <= 0) {
        return;
    }
    for (i = 0; i < n; i++) {
        int len = dx_strlen(labels[i]);

        dx_gauge_init(&gs[i], labels[i], values[i], maxes[i]);
        if (len > label_cols) {
            label_cols = len;
        }
    }
    value_cols = dx_gauge_value_cols(gs, n);
    for (i = 0; i < n && i * pitch + DX_ROW_H <= area.h; i++) {
        dx_rect_t row = { area.x, area.y + i * pitch, area.w, DX_ROW_H };

        dx_gauge_draw(&gs[i], ctx, row, label_cols, value_cols);
    }
}
