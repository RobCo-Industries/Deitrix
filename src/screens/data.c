/*
 * The DATA tab: three sub-tabs. NOTES lists the notes on the card and
 * opens one, TAPES lists and runs the holotapes, CLOCK shows the time
 * and sets it. Leaving the tab closes an open note or the setter and
 * keeps the sub-tab and the selections.
 */
#include "screens/screens.h"

#include "content.h"
#include "model/util.h"
#include "screens/data_view.h"
#include "ui/list.h"
#include "ui/subtabs.h"
#include "ui/text.h"

enum { SUB_NOTES, SUB_TAPES, SUB_CLOCK, SUB_COUNT };

static const char *const sub_labels[SUB_COUNT] = { "NOTES", "TAPES", "CLOCK" };

/* The open note: its title row, a faint rule, then the body. */
#define NOTE_TITLE_DY 4
#define NOTE_RULE_DY  22
#define NOTE_TEXT_DY  26

#define LOG_SIZE 128

typedef struct data_state {
    int ready;
    dx_subtabs_t subs;
    dx_list_t notes;
    int open;      /* 1 while a note is open */
    int open_ok;   /* its body was read */
    dx_text_t text;
    char body[DX_NOTE_FILE_MAX];
    char logged[LOG_SIZE];
} data_state_t;

static data_state_t data;

void dx_data_message(const dx_ctx_t *ctx, dx_rect_t r, const char *a, const char *b)
{
    int y = r.y + (r.h - DX_ROW_H) / 2 - (b != NULL ? DX_ROW_H / 2 : 0);

    ctx->gfx->text(r.x + (r.w - dx_text_width(a, 1)) / 2, y, a, ctx->theme.dim, DX_COLOR_NONE,
                   1);
    if (b != NULL) {
        ctx->gfx->text(r.x + (r.w - dx_text_width(b, 1)) / 2, y + DX_ROW_H, b, ctx->theme.dim,
                       DX_COLOR_NONE, 1);
    }
}

void dx_data_log(const char *line)
{
    if (dx_str_eq(line, data.logged)) {
        return;
    }
    dx_str_cut(data.logged, sizeof(data.logged), line, dx_str_len(line));
    dx_plat_log(line);
}

void dx_data_log_reset(void)
{
    data.logged[0] = '\0';
}

static const dx_notes_t *notes(void)
{
    return dx_content_notes();
}

static void note_row(void *user, int i, dx_row_t *out)
{
    (void)user;
    out->label = notes()->notes[i].title;
    out->value = NULL;
    out->flags = 0;
}

static dx_rect_t below_tabs(dx_rect_t area)
{
    dx_rect_t r = { area.x, area.y + DX_SUBTABS_H, area.w, area.h - DX_SUBTABS_H };

    return r;
}

static void setup(void)
{
    const dx_notes_t *n = notes();
    int count = n->load.status == DX_LOAD_OK ? n->count : 0;
    int h = DX_BODY_H - DX_SUBTABS_H - DX_DATA_LIST_DY;

    if (!data.ready) {
        data.ready = 1;
        dx_subtabs_init(&data.subs, sub_labels, SUB_COUNT);
        dx_list_init(&data.notes, count, h / DX_ROW_H, note_row, NULL, "No notes on the card.");
    }
    /* The shell enters the first tab before the content loads. */
    if (data.notes.count != count) {
        data.notes.count = count;
        dx_list_select(&data.notes, data.notes.sel);
    }
}

void dx_data_reset(void)
{
    dx_mem_set(&data, 0, sizeof(data));
    dx_data_tapes_reset();
    dx_data_clock_reset();
}

static void enter(void *st)
{
    (void)st;
    setup();
    data.logged[0] = '\0';
    if (data.subs.active == SUB_TAPES) {
        dx_data_tapes_enter();
    }
}

static void leave(void *st)
{
    (void)st;
    data.open = 0;
    data.logged[0] = '\0';
    dx_data_tapes_leave();
    dx_data_clock_leave();
}

/* Opens the selected note: its body into the screen's buffer. */
static void open_note(void)
{
    int n = dx_content_note_body(data.notes.sel, data.body, sizeof(data.body));
    dx_rect_t r = below_tabs((dx_rect_t){ 0, DX_BODY_TOP, DX_SCREEN_W, DX_BODY_H });

    data.open = 1;
    data.open_ok = n >= 0;
    if (n < 0) {
        data.body[0] = '\0';
    }
    /* The full width: the text widget pads 4 px and keeps 38 columns. */
    dx_text_init(&data.text, data.body, r.w, r.h - NOTE_TEXT_DY);
}

static unsigned note_event(const dx_ui_event_t *ev)
{
    if (dx_ui_is_back(ev)) {
        data.open = 0;
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    if (ev->type == DX_UI_RIGHT) {
        return DX_UI_HANDLED;
    }
    return dx_text_event(&data.text, ev);
}

static unsigned event(void *st, const dx_ui_event_t *ev)
{
    unsigned r;
    int was;

    (void)st;
    setup();
    if (data.subs.active == SUB_NOTES && data.open) {
        return note_event(ev);
    }
    if (data.subs.active == SUB_CLOCK && dx_data_clock_nested()) {
        return dx_data_clock_event(ev);
    }
    was = data.subs.active;
    r = dx_subtabs_event(&data.subs, ev);
    if (r != DX_UI_IGNORED) {
        if (was == SUB_TAPES) {
            dx_data_tapes_leave();
        }
        if (was == SUB_CLOCK) {
            dx_data_clock_leave();
        }
        if (data.subs.active == SUB_TAPES) {
            dx_data_tapes_enter();
        }
        return r;
    }
    switch (data.subs.active) {
    case SUB_TAPES:
        return dx_data_tapes_event(ev);
    case SUB_CLOCK:
        return dx_data_clock_event(ev);
    default:
        break;
    }
    if (ev->type == DX_UI_SELECT && data.notes.count > 0) {
        open_note();
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    return dx_list_event(&data.notes, ev);
}

static uint32_t tick(void *st, uint32_t now_ms, unsigned *flags)
{
    (void)st;
    setup();
    if (data.subs.active != SUB_CLOCK) {
        return 0;
    }
    return dx_data_clock_tick(now_ms, flags);
}

static void draw_list(const dx_ctx_t *ctx, dx_rect_t r)
{
    const dx_notes_t *n = notes();
    char line[LOG_SIZE];
    char why[40];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: data notes sel=");
    dx_sb_int(&sb, data.notes.sel);
    dx_sb_str(&sb, " top=");
    dx_sb_int(&sb, data.notes.top);
    dx_sb_str(&sb, " count=");
    dx_sb_int(&sb, data.notes.count);
    dx_sb_char(&sb, '\n');
    dx_data_log(line);
    if (n->load.status != DX_LOAD_OK && n->load.status != DX_LOAD_MISSING) {
        dx_data_message(ctx, r, "The notes were not read.",
                        dx_placeholder_reason(&n->load, "NOTES", why, sizeof(why)));
        return;
    }
    dx_list_draw(&data.notes, ctx,
                 (dx_rect_t){ r.x, r.y + DX_DATA_LIST_DY, r.w, r.h - DX_DATA_LIST_DY });
}

static void draw_note(const dx_ctx_t *ctx, dx_rect_t r)
{
    const dx_notefile_t *f = &notes()->notes[data.notes.sel];
    dx_rect_t body = { r.x, r.y + NOTE_TEXT_DY, r.w, r.h - NOTE_TEXT_DY };
    char line[LOG_SIZE];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: data note ");
    dx_sb_str(&sb, f->file);
    dx_sb_str(&sb, data.open_ok ? " top=" : " error top=");
    dx_sb_int(&sb, data.text.top);
    dx_sb_str(&sb, " lines=");
    dx_sb_int(&sb, data.text.count);
    dx_sb_str(&sb, data.text.cut ? " cut\n" : "\n");
    dx_data_log(line);
    dx_text_cut(ctx, r.x + 4, r.y + NOTE_TITLE_DY, f->title, (r.w - 8) / DX_FONT_W,
                ctx->theme.fg, DX_COLOR_NONE);
    ctx->gfx->line(r.x + 4, r.y + NOTE_RULE_DY, r.x + r.w - 5, r.y + NOTE_RULE_DY,
                   ctx->theme.faint);
    if (!data.open_ok) {
        dx_data_message(ctx, body, "The note could not be read.", NULL);
        return;
    }
    dx_text_draw(&data.text, ctx, body);
}

static void draw(void *st, const dx_ctx_t *ctx, dx_rect_t area)
{
    dx_rect_t r = below_tabs(area);

    (void)st;
    setup();
    dx_subtabs_draw(&data.subs, ctx, area);
    switch (data.subs.active) {
    case SUB_TAPES:
        dx_data_tapes_draw(ctx, r);
        break;
    case SUB_CLOCK:
        dx_data_clock_draw(ctx, r);
        break;
    default:
        if (data.open) {
            draw_note(ctx, r);
        } else {
            draw_list(ctx, r);
        }
        break;
    }
}

static const char *hint(void *st)
{
    (void)st;
    setup();
    switch (data.subs.active) {
    case SUB_TAPES:
        return dx_data_tapes_hint();
    case SUB_CLOCK:
        return dx_data_clock_hint();
    default:
        break;
    }
    if (data.open) {
        return data.text.count > data.text.visible ? "UP/DOWN: scroll   BACK/LEFT: list"
                                                   : "BACK/LEFT: list   DIAL: tab";
    }
    return data.notes.count > 0 ? "UP/DOWN: move   SELECT: open"
                                : "RIGHT: TAPES   LEFT or DIAL: tab";
}

const dx_screen_t dx_screen_data = {
    "DATA", enter, leave, event, tick, draw, hint, NULL,
};
