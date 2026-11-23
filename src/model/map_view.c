#include "model/map_view.h"

#include "model/util.h"

dx_map_fit_t dx_map_pane_fit(const dx_map_t *m, int x, int y, int w, int h)
{
    return dx_map_fit(m->box, x + DX_MAP_MARGIN, y + DX_MAP_MARGIN, w - 2 * DX_MAP_MARGIN,
                      h - 2 * DX_MAP_MARGIN);
}

int dx_map_line_px(const dx_map_fit_t *f, const dx_polyline_t *l, int16_t *xy)
{
    int n = l->count < DX_POINTS_MAX ? l->count : DX_POINTS_MAX;
    int i;

    for (i = 0; i < n; i++) {
        int px, py;

        dx_map_to_px(f, l->pts[i].x, l->pts[i].y, &px, &py);
        xy[2 * i] = (int16_t)px;
        xy[2 * i + 1] = (int16_t)py;
    }
    return n;
}

int dx_map_cycle(int sel, int count, int step)
{
    if (count <= 0) {
        return -1;
    }
    if (sel < 0 || sel >= count) {
        return 0;
    }
    if (step < 0) {
        return sel == 0 ? count - 1 : sel - 1;
    }
    return sel == count - 1 ? 0 : sel + 1;
}

dx_map_square_t dx_map_square(int px, int py, int size)
{
    dx_map_square_t s;

    s.x = px - size / 2;
    s.y = py - size / 2;
    s.size = size;
    return s;
}

const char *dx_map_pos_text(dx_point_t p, char *buf)
{
    dx_sb_t sb;

    dx_sb_init(&sb, buf, DX_MAP_POS_SIZE);
    dx_sb_int(&sb, p.x);
    dx_sb_str(&sb, ", ");
    dx_sb_int(&sb, p.y);
    return buf;
}
