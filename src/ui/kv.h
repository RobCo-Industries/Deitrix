/* Read-only key and value rows, the values aligned in one column. */
#ifndef DX_UI_KV_H
#define DX_UI_KV_H

#include "ui/ui.h"

typedef struct dx_kv_row {
    const char *key;
    const char *value;
} dx_kv_row_t;

typedef struct dx_kv {
    const dx_kv_row_t *rows;
    int count;
    int key_cols; /* the longest key, at most half the columns */
} dx_kv_t;

void dx_kv_init(dx_kv_t *kv, const dx_kv_row_t *rows, int count);

/* Draws as many rows as fit, one per DX_ROW_H. */
void dx_kv_draw(const dx_kv_t *kv, const dx_ctx_t *ctx, dx_rect_t area);

#endif
