/*
 * The INV tab: the items of ITEMS.INI, one sub-tab per category. A
 * category shows its items in a list with a summary row under it.
 * SELECT opens an item's detail, where UP and DOWN change its count and
 * SELECT marks it. Every change goes through the model's item
 * functions; the autosave writes INV.SAV.
 *
 * Each key the tab takes logs one "deitrix: inv ..." line before the
 * redraw, so the QEMU tests can follow it.
 */
#include "screens/screens.h"
#include "screens/inv_screen.h"

#include "content.h"
#include "model/inv_fmt.h"
#include "ui/frame.h"
#include "ui/kv.h"
#include "ui/list.h"
#include "ui/subtabs.h"
#include "ui/text.h"

#define PAD        4
#define NOTE_LINES 12 /* more than the body holds */
#define LOG_CAP    48 /* "deitrix: inv open=" and an id of 12 */

static struct {
    int cat;                 /* the category shown */
    int sel[DX_CATS_MAX];    /* the selected row of each category */
    int top[DX_CATS_MAX];    /* the first row in its window */
    int detail;              /* the model index shown in detail, or -1 */
} s = { 0, { 0 }, { 0 }, -1 };

/* 1 once the current key logged its line. */
static int logged;

void dx_inv_screen_reset(void)
{
    int i;

    s.cat = 0;
    for (i = 0; i < DX_CATS_MAX; i++) {
        s.sel[i] = 0;
        s.top[i] = 0;
    }
    s.detail = -1;
}

/* 1 when there is at least one category to show. */
static int have_items(const dx_items_t *it)
{
    return it->load.status == DX_LOAD_OK && it->cat_count > 0;
}

static void log_line(const char *what, const char *text, int32_t n)
{
    char line[LOG_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: inv ");
    dx_sb_str(&sb, what);
    if (text != NULL) {
        dx_sb_str(&sb, text);
    } else if (n >= 0) {
        dx_sb_int(&sb, n);
    }
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
    logged = 1;
}

static void log_row(void)
{
    char line[LOG_CAP];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: inv cat=");
    dx_sb_int(&sb, s.cat);
    dx_sb_str(&sb, " row=");
    dx_sb_int(&sb, s.sel[s.cat]);
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
    logged = 1;
}

/* ---- The list ------------------------------------------------------------ */

static char row_value[DX_INV_COUNT_CAP];

static void item_row(void *user, int index, dx_row_t *out)
{
    const dx_items_t *it = dx_content_items();
    int i = dx_inv_item_at(it, *(const int *)user, index);
    dx_sb_t sb;

    if (i < 0) {
        return;
    }
    dx_sb_init(&sb, row_value, sizeof(row_value));
    dx_inv_fmt_count(&sb, &it->items[i]);
    out->label = it->items[i].name;
    out->value = row_value;
    out->flags = (it->items[i].count == 0 ? DX_ROW_DISABLED : 0u) |
                 (it->items[i].marked ? DX_ROW_MARKED : 0u);
}

/* The list of the current category, at its kept selection. */
static void make_list(const dx_items_t *it, dx_list_t *l)
{
    dx_list_init(l, dx_inv_cat_entries(it, s.cat), DX_INV_LIST_ROWS, item_row, &s.cat,
                 "Nothing here.");
    l->top = s.top[s.cat];
    dx_list_select(l, s.sel[s.cat]);
}

static void make_tabs(const dx_items_t *it, dx_subtabs_t *t)
{
    const char *labels[DX_CATS_MAX];
    int i;

    for (i = 0; i < it->cat_count && i < DX_CATS_MAX; i++) {
        labels[i] = it->cats[i].label;
    }
    dx_subtabs_init(t, labels, it->cat_count);
    t->active = s.cat;
}

/* Keeps the state inside the content: the ROM loads it once, but the
 * host tests load several. */
static void clamp(const dx_items_t *it)
{
    dx_list_t l;

    if (s.cat >= it->cat_count) {
        s.cat = 0;
    }
    make_list(it, &l);
    s.sel[s.cat] = l.sel;
    s.top[s.cat] = l.top;
    if (s.detail >= it->item_count) {
        s.detail = -1;
    }
}

static unsigned list_event(const dx_items_t *it, const dx_ui_event_t *ev)
{
    dx_subtabs_t t;
    dx_list_t l;
    unsigned r;

    if (ev->type == DX_UI_LEFT || ev->type == DX_UI_RIGHT) {
        make_tabs(it, &t);
        r = dx_subtabs_event(&t, ev);
        if (r & DX_UI_HANDLED) {
            s.cat = t.active;
            log_row();
        }
        return r;
    }
    make_list(it, &l);
    if (ev->type == DX_UI_SELECT) {
        if (l.count == 0 || ev->repeat) {
            return DX_UI_IGNORED;
        }
        s.detail = dx_inv_item_at(it, s.cat, l.sel);
        log_line("open=", it->items[s.detail].id, -1);
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    r = dx_list_event(&l, ev);
    if (r & DX_UI_HANDLED) {
        s.sel[s.cat] = l.sel;
        s.top[s.cat] = l.top;
        log_row();
    } else if (ev->type == DX_UI_UP || ev->type == DX_UI_DOWN) {
        log_line("end row=", NULL, l.sel);
    }
    return r;
}

/* ---- The detail ---------------------------------------------------------- */

static unsigned detail_event(dx_items_t *it, const dx_ui_event_t *ev)
{
    const dx_item_t *item = &it->items[s.detail];

    if (dx_ui_is_back(ev)) {
        s.detail = -1;
        log_line("back row=", NULL, s.sel[s.cat]);
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    switch (ev->type) {
    case DX_UI_UP:
    case DX_UI_DOWN:
        /* One step per press or repeat. At 0 or 999 the model refuses:
         * the shell plays the deny tone for a press. */
        if (!dx_items_add(it, s.detail, ev->type == DX_UI_UP ? 1 : -1)) {
            log_line("refused count=", NULL, item->count);
            return DX_UI_IGNORED;
        }
        log_line("count=", NULL, item->count);
        return DX_UI_HANDLED | DX_UI_DIRTY;
    case DX_UI_SELECT:
        if (ev->repeat) {
            return DX_UI_IGNORED;
        }
        dx_items_toggle_marked(it, s.detail);
        log_line("marked=", NULL, item->marked);
        return DX_UI_HANDLED | DX_UI_DIRTY;
    case DX_UI_RIGHT:
        /* Handled, so it cannot change the tab from the detail. */
        return DX_UI_HANDLED;
    default:
        return DX_UI_IGNORED;
    }
}

static void draw_note(const dx_ctx_t *ctx, dx_rect_t area, const char *note)
{
    dx_line_t lines[NOTE_LINES];
    char buf[DX_SCREEN_W / DX_FONT_W + 1];
    int cols = (area.w - 2 * PAD) / DX_FONT_W;
    int fit = area.h / DX_ROW_H;
    int cut = 0;
    int n = dx_wrap(note, cols, lines, NOTE_LINES, &cut);
    int i;

    if (n > fit) {
        n = fit;
        cut = 1;
    }
    for (i = 0; i < n; i++) {
        int len = lines[i].len;
        int k;

        if (len > cols) {
            len = cols;
        }
        for (k = 0; k < len; k++) {
            buf[k] = note[lines[i].start + (uint32_t)k];
        }
        if (cut && i == n - 1) {
            /* The last line that fits ends with the cut marker. */
            if (len > cols - 1) {
                len = cols - 1;
            }
            buf[len++] = '~';
        }
        buf[len] = '\0';
        ctx->gfx->text(area.x + PAD, area.y + i * DX_ROW_H, buf, ctx->theme.dim, DX_COLOR_NONE,
                       1);
    }
}

static void draw_detail(const dx_items_t *it, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_item_t *item = &it->items[s.detail];
    char count[DX_INV_COUNT_CAP];
    char weight[DX_INV_WEIGHT_CAP];
    dx_kv_row_t rows[4];
    dx_kv_t kv;
    dx_sb_t sb;
    int n = 0;
    dx_rect_t r;

    dx_text_cut(ctx, area.x + PAD, area.y + 2, item->name, (area.w - 2 * PAD) / DX_FONT_W,
                ctx->theme.fg, DX_COLOR_NONE);
    ctx->gfx->line(area.x, area.y + DX_SUBTABS_H - 1, area.x + area.w - 1,
                   area.y + DX_SUBTABS_H - 1, ctx->theme.faint);
    dx_sb_init(&sb, count, sizeof(count));
    dx_inv_fmt_count(&sb, item);
    rows[n++] = (dx_kv_row_t){ "Count", count };
    if (item->weight != DX_WEIGHT_NONE) {
        dx_sb_init(&sb, weight, sizeof(weight));
        dx_inv_fmt_weight(&sb, item->weight);
        rows[n++] = (dx_kv_row_t){ "Weight", weight };
    }
    rows[n++] = (dx_kv_row_t){ "Category", it->cats[item->cat].label };
    rows[n++] = (dx_kv_row_t){ "Marked", item->marked ? "Yes" : "No" };
    dx_kv_init(&kv, rows, n);
    r = (dx_rect_t){ area.x, area.y + DX_INV_KV_DY, area.w, n * DX_ROW_H };
    dx_kv_draw(&kv, ctx, r);
    if (item->note[0] != '\0') {
        r.y += r.h + DX_INV_NOTE_GAP;
        r.h = area.y + area.h - r.y;
        draw_note(ctx, r, item->note);
    }
}

/* ---- The screen ---------------------------------------------------------- */

static void leave(void *st)
{
    (void)st;
    /* The category and the row stay; the detail closes. */
    s.detail = -1;
}

static unsigned event(void *st, const dx_ui_event_t *ev)
{
    dx_items_t *it = dx_content_items();
    unsigned r;

    (void)st;
    logged = 0;
    if (!have_items(it)) {
        r = DX_UI_IGNORED;
    } else {
        clamp(it);
        r = s.detail >= 0 ? detail_event(it, ev) : list_event(it, ev);
    }
    if (!logged && !(r == DX_UI_IGNORED && (ev->type == DX_UI_LEFT || ev->type == DX_UI_RIGHT))) {
        /* A key that changed nothing. A test waits for this line to
         * know the frame before it is on screen. LEFT or RIGHT that the
         * shell turns into a tab change logs the tab instead. */
        log_line("idle", NULL, -1);
    }
    return r;
}

static void draw_empty(const dx_items_t *it, const dx_ctx_t *ctx, dx_rect_t area)
{
    char reason[40];
    int y = area.y + (area.h - DX_ROW_H) / 2;

    dx_text_centered(ctx, y - DX_ROW_H / 2, "No items on the card.", ctx->theme.fg, 1);
    if (it->load.status != DX_LOAD_OK) {
        dx_placeholder_reason(&it->load, "ITEMS.INI", reason, sizeof(reason));
        dx_text_centered(ctx, y + DX_ROW_H, reason, ctx->theme.dim, 1);
    }
}

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    const dx_items_t *it = dx_content_items();
    char summary[DX_INV_SUMMARY_CAP];
    dx_subtabs_t t;
    dx_list_t l;
    dx_sb_t sb;
    int y;

    (void)st;
    if (!have_items(it)) {
        draw_empty(it, ctx, area);
        return;
    }
    clamp(it);
    if (s.detail >= 0) {
        draw_detail(it, ctx, area);
        return;
    }
    make_tabs(it, &t);
    dx_subtabs_draw(&t, ctx, area);
    make_list(it, &l);
    dx_list_draw(&l, ctx,
                 (dx_rect_t){ area.x, area.y + DX_INV_LIST_DY, area.w,
                              DX_INV_LIST_ROWS * DX_ROW_H });
    if (l.count == 0) {
        return;
    }
    y = area.y + DX_INV_RULE_DY;
    ctx->gfx->line(area.x, y, area.x + area.w - 1, y, ctx->theme.faint);
    dx_sb_init(&sb, summary, sizeof(summary));
    dx_inv_fmt_summary(&sb, it, s.cat);
    ctx->gfx->text(area.x + 12, area.y + DX_INV_SUMMARY_DY, summary, ctx->theme.dim,
                   DX_COLOR_NONE, 1);
}

static const char *hint(void *st)
{
    const dx_items_t *it = dx_content_items();

    (void)st;
    if (!have_items(it)) {
        return NULL;
    }
    if (s.detail >= 0) {
        return "UP/DOWN: count  SELECT: mark  BACK";
    }
    if (dx_inv_cat_entries(it, s.cat) == 0) {
        return "LEFT/RIGHT: category  DIAL: tab";
    }
    return "UP/DOWN: item  SELECT: open";
}

const dx_screen_t dx_screen_inv = {
    "INV", NULL, leave, event, NULL, draw, hint, NULL,
};
