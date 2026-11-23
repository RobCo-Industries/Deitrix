/* A block of wrapped text, scrolled one line at a time. */
#ifndef DX_UI_TEXT_H
#define DX_UI_TEXT_H

#include "ui/ui.h"

/*
 * The limits of a text block. The widget keeps at most
 * DX_TEXT_MAX_LINES lines in its fixed table and reads at most
 * DX_TEXT_MAX_BYTES bytes of the text. A longer text is cut at the
 * first limit it reaches: the last line of the table then shows
 * DX_TEXT_CUT_MARK instead, and `cut` is 1.
 */
#define DX_TEXT_MAX_LINES 128
#define DX_TEXT_MAX_BYTES (256 * 1024)
#define DX_TEXT_CUT_MARK  "[text cut]"

/* One wrapped line: `len` bytes of the source from `start`. A line is
 * at most 40 columns, so `len` fits 16 bits; `start` does not wrap
 * below DX_TEXT_MAX_BYTES. */
typedef struct dx_line {
    uint32_t start;
    uint16_t len;
} dx_line_t;

/*
 * Wraps s to lines of at most `cols` characters, breaking on spaces.
 * A word longer than a line is split. '\n' forces a break. A tab
 * counts as one space. Spaces at a wrap point are dropped. Reads at
 * most DX_TEXT_MAX_BYTES bytes and writes at most `max` lines. Returns
 * the lines written. Sets *cut, when cut is not NULL, to 1 when text
 * was left out by either limit, else 0. Pure.
 */
int dx_wrap(const char *s, int cols, dx_line_t *lines, int max, int *cut);

typedef struct dx_text {
    const char *s;
    dx_line_t lines[DX_TEXT_MAX_LINES];
    int count;   /* lines kept, the mark included, at most DX_TEXT_MAX_LINES */
    int cut;     /* 1 when the text was cut: the last line is the mark */
    int visible; /* lines in the window */
    int top;     /* the first line in the window */
} dx_text_t;

/* Wraps s for an area `w` px wide and `h` px high, scroll bar included. */
void dx_text_init(dx_text_t *t, const char *s, int w, int h);

/* UP and DOWN scroll one line, repeats too; DX_UI_IGNORED at the ends. */
unsigned dx_text_event(dx_text_t *t, const dx_ui_event_t *ev);

void dx_text_draw(const dx_text_t *t, const dx_ctx_t *ctx, dx_rect_t area);

#endif
