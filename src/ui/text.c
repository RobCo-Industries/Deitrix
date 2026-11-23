#include "ui/text.h"

#define PAD_L 4
#define LINE_MAX_COLS 40

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

int dx_wrap(const char *s, int cols, dx_line_t *lines, int max)
{
    int n = 0;
    int p = 0;
    int fresh = 1; /* at the start of the text or after '\n' */

    if (cols < 1) {
        return 0;
    }
    while (s[p] != '\0') {
        int start, end, brk;

        if (!fresh) {
            /* A wrapped line starts at its first word. */
            while (is_space(s[p])) {
                p++;
            }
            if (s[p] == '\0') {
                break;
            }
        }
        start = p;
        brk = -1; /* the last space inside the line */
        while (s[p] != '\0' && s[p] != '\n' && p - start < cols) {
            if (is_space(s[p])) {
                brk = p;
            }
            p++;
        }
        if (s[p] == '\0' || s[p] == '\n') {
            end = p;
            fresh = s[p] == '\n';
            if (s[p] == '\n') {
                p++;
            }
        } else if (is_space(s[p])) {
            end = p; /* the line is full and a space follows: a clean break */
            fresh = 0;
        } else if (brk >= 0) {
            end = brk; /* break at the last space */
            p = brk;
            fresh = 0;
        } else {
            end = p; /* one word longer than the line: split it */
            fresh = 0;
        }
        while (end > start && is_space(s[end - 1])) {
            end--;
        }
        if (n < max) {
            lines[n].start = (uint16_t)start;
            lines[n].len = (uint16_t)(end - start);
        }
        n++;
    }
    return n;
}

void dx_text_init(dx_text_t *t, const char *s, int w, int h)
{
    int cols = (w - PAD_L - DX_SCROLLBAR_W - 2) / DX_FONT_W;
    int n;

    if (cols > LINE_MAX_COLS) {
        cols = LINE_MAX_COLS;
    }
    n = dx_wrap(s, cols, t->lines, DX_TEXT_MAX_LINES);
    t->s = s;
    t->count = n < DX_TEXT_MAX_LINES ? n : DX_TEXT_MAX_LINES;
    t->visible = h / DX_ROW_H;
    if (t->visible < 1) {
        t->visible = 1;
    }
    t->top = 0;
}

unsigned dx_text_event(dx_text_t *t, const dx_ui_event_t *ev)
{
    int last = t->count - t->visible;

    if (ev->type == DX_UI_UP && t->top > 0) {
        t->top--;
    } else if (ev->type == DX_UI_DOWN && t->top < last) {
        t->top++;
    } else {
        return DX_UI_IGNORED;
    }
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

void dx_text_draw(const dx_text_t *t, const dx_ctx_t *ctx, dx_rect_t area)
{
    int rows = t->visible;
    int i;

    if (rows > area.h / DX_ROW_H) {
        rows = area.h / DX_ROW_H;
    }
    for (i = 0; i < rows && t->top + i < t->count; i++) {
        const dx_line_t *ln = &t->lines[t->top + i];
        char buf[LINE_MAX_COLS + 1];
        int j;

        if (ln->len == 0) {
            continue;
        }
        for (j = 0; j < ln->len && j < LINE_MAX_COLS; j++) {
            char c = t->s[ln->start + j];

            buf[j] = c == '\t' ? ' ' : c;
        }
        buf[j] = '\0';
        ctx->gfx->text(area.x + PAD_L, area.y + i * DX_ROW_H, buf, ctx->theme.fg, DX_COLOR_NONE,
                       1);
    }
    if (t->count > rows) {
        dx_rect_t bar = { area.x + area.w - DX_SCROLLBAR_W, area.y, DX_SCROLLBAR_W,
                          rows * DX_ROW_H };

        dx_scrollbar(ctx, bar, t->top, rows, t->count);
    }
}
