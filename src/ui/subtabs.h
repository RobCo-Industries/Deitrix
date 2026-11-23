/* A row of up to five short labels under the header. */
#ifndef DX_UI_SUBTABS_H
#define DX_UI_SUBTABS_H

#include "ui/ui.h"

#define DX_SUBTABS_MAX 5
#define DX_SUBTABS_H   20 /* the row and its rule */

typedef struct dx_subtabs {
    const char *labels[DX_SUBTABS_MAX];
    int count;
    int active;
} dx_subtabs_t;

/* Takes at most DX_SUBTABS_MAX labels; the first is active. */
void dx_subtabs_init(dx_subtabs_t *st, const char *const *labels, int count);

/* LEFT and RIGHT move by one, repeats too. At either end they return
 * DX_UI_IGNORED, so the app changes the main tab. */
unsigned dx_subtabs_event(dx_subtabs_t *st, const dx_ui_event_t *ev);

/* Draws the row at the top of area: the active label bright and
 * underlined, the others dim, then a faint rule. */
void dx_subtabs_draw(const dx_subtabs_t *st, const dx_ctx_t *ctx, dx_rect_t area);

#endif
