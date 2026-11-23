/* A block of wrapped text, scrolled one line at a time. */
#ifndef DX_UI_TEXT_H
#define DX_UI_TEXT_H

#include "ui/ui.h"

#define DX_TEXT_MAX_LINES 128

/* One wrapped line: `len` bytes of the source from `start`. */
typedef struct dx_line {
    uint16_t start;
    uint16_t len;
} dx_line_t;

/*
 * Wraps s to lines of at most `cols` characters, breaking on spaces.
 * A word longer than a line is split. '\n' forces a break. A tab
 * counts as one space. Spaces at a wrap point are dropped. Writes at
 * most `max` lines and returns the count it would need. Pure.
 */
int dx_wrap(const char *s, int cols, dx_line_t *lines, int max);

typedef struct dx_text {
    const char *s;
    dx_line_t lines[DX_TEXT_MAX_LINES];
    int count;   /* lines kept, at most DX_TEXT_MAX_LINES */
    int visible; /* lines in the window */
    int top;     /* the first line in the window */
} dx_text_t;

/* Wraps s for an area `w` px wide and `h` px high, scroll bar included. */
void dx_text_init(dx_text_t *t, const char *s, int w, int h);

/* UP and DOWN scroll one line, repeats too; DX_UI_IGNORED at the ends. */
unsigned dx_text_event(dx_text_t *t, const dx_ui_event_t *ev);

void dx_text_draw(const dx_text_t *t, const dx_ctx_t *ctx, dx_rect_t area);

#endif
