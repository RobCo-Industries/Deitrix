#include "ui/text.h"

#define PAD_L 4
#define LINE_MAX_COLS 40

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

/* The end of the readable text: its '\0' or the byte limit. */
static int at_end(const char *s, int p)
{
    return p >= DX_TEXT_MAX_BYTES || s[p] == '\0';
}

int dx_wrap(const char *s, int cols, dx_line_t *lines, int max, int *cut)
{
    int n = 0;
    int p = 0;
    int fresh = 1; /* at the start of the text or after '\n' */
    int over = 0;

    if (cols < 1 || max < 0) {
        max = 0;
    }
    while (!at_end(s, p)) {
        int start, end, brk;

        if (!fresh) {
            /* A wrapped line starts at its first word. */
            while (!at_end(s, p) && is_space(s[p])) {
                p++;
            }
            if (at_end(s, p)) {
                break;
            }
        }
        if (n >= max) {
            over = cols >= 1; /* a line that does not fit the table */
            break;
        }
        start = p;
        brk = -1; /* the last space inside the line */
        while (!at_end(s, p) && s[p] != '\n' && p - start < cols) {
            if (is_space(s[p])) {
                brk = p;
            }
            p++;
        }
        if (at_end(s, p) || s[p] == '\n') {
            end = p;
            fresh = !at_end(s, p);
            if (fresh) {
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
        lines[n].start = (uint32_t)start;
        lines[n].len = (uint16_t)(end - start);
        n++;
    }
    /* At the byte limit with text left: s[p] is readable, no '\0' came before. */
    if (p >= DX_TEXT_MAX_BYTES && s[p] != '\0') {
        over = 1;
    }
    if (cut != NULL) {
        *cut = over;
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
    n = dx_wrap(s, cols, t->lines, DX_TEXT_MAX_LINES, &t->cut);
    if (t->cut && n == DX_TEXT_MAX_LINES) {
        n--; /* room for the mark */
    }
    t->s = s;
    t->count = n + t->cut;
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

        if (t->cut && t->top + i == t->count - 1) {
            ctx->gfx->text(area.x + PAD_L, area.y + i * DX_ROW_H, DX_TEXT_CUT_MARK, ctx->theme.dim,
                           DX_COLOR_NONE, 1);
            continue;
        }
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
