/*
 * A selectable list of rows, one font cell high. A callback supplies
 * the rows, so the list holds no row data.
 */
#ifndef DX_UI_LIST_H
#define DX_UI_LIST_H

#include "ui/ui.h"

#define DX_ROW_DISABLED 1u /* drawn dim */
#define DX_ROW_MARKED   2u /* drawn with a mark at the left */

typedef struct dx_row {
    const char *label;
    const char *value; /* right-aligned, or NULL */
    unsigned flags;    /* DX_ROW_* */
} dx_row_t;

typedef void (*dx_row_fn)(void *user, int index, dx_row_t *out);

typedef struct dx_list {
    dx_row_fn row;
    void *user;
    const char *empty; /* drawn centred when count is 0 */
    int count;
    int visible; /* rows in the window */
    int sel;     /* the selected row */
    int top;     /* the first row in the window */
} dx_list_t;

/* `visible` is the window: the area height / DX_ROW_H. */
void dx_list_init(dx_list_t *l, int count, int visible, dx_row_fn row, void *user,
                  const char *empty);

/*
 * The first row of a window of `visible` rows that shows `sel`, moving
 * the window from `top` as little as possible. Pure.
 */
int dx_list_window(int sel, int top, int count, int visible);

/* Selects row `sel`, clamped, and moves the window to show it. */
void dx_list_select(dx_list_t *l, int sel);

/*
 * UP and DOWN move the selection by one, repeats too. At either end the
 * list does not move and returns DX_UI_IGNORED. Other events: ignored.
 */
unsigned dx_list_event(dx_list_t *l, const dx_ui_event_t *ev);

/* Draws the window from area's top left; a scroll bar at the right
 * when the rows do not all fit. */
void dx_list_draw(const dx_list_t *l, const dx_ctx_t *ctx, dx_rect_t area);

#endif
