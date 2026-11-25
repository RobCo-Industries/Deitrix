/* A labelled horizontal bar for `value` of `max`, the numbers in a column
 * at the right. Selected, the row is drawn inverse, as a list draws its
 * selected row. */
#ifndef DX_UI_GAUGE_H
#define DX_UI_GAUGE_H

#include "ui/ui.h"

typedef struct dx_gauge {
    const char *label;
    int32_t value;
    int32_t max;
    int selected;   /* 1: inverse, the whole area filled with fg */
    int value_only; /* 1: the text is "value", not "value/max" */
    int pad;        /* the label's x from the area's left edge */
} dx_gauge_t;

/* A plain gauge: not selected, "value/max", a pad of 4 px. */
void dx_gauge_init(dx_gauge_t *g, const char *label, int32_t value, int32_t max);

/* value clamped to 0 .. max; 0 when max is 0 or less. Pure. */
int32_t dx_gauge_clamp(int32_t value, int32_t max);

/* The filled width of a bar `width` px wide. Pure. */
int dx_gauge_fill(int32_t value, int32_t max, int width);

/* Writes "value/max", or "value" when value_only, clamped, into buf,
 * which holds 24. Returns the length. */
int dx_gauge_text(const dx_gauge_t *g, char *buf);

/* The longest "value/max" text of n gauges: the value_cols that lines
 * them up. */
int dx_gauge_value_cols(const dx_gauge_t *gs, int n);

/*
 * Draws one DX_ROW_H row at the top of area: the label in its first
 * `label_cols` columns, the bar, then "value/max" right-aligned in the
 * last `value_cols` columns. Gauges drawn with the same area width,
 * label_cols and value_cols share both bar edges. A text longer than
 * value_cols widens its own column and shortens its bar. The label
 * starts g->pad px from the left. Selected, the row is filled with fg
 * and the label, bar and text are drawn in bg on it.
 */
void dx_gauge_draw(const dx_gauge_t *g, const dx_ctx_t *ctx, dx_rect_t area, int label_cols,
                   int value_cols);

#endif
