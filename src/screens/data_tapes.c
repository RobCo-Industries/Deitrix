/*
 * The DATA tab's TAPES sub-tab: the holotapes Pip-OS lists, the
 * selected one's icon, and a launcher. Before a launch the autosave
 * writes what waits, so a tape that crashes the device loses nothing,
 * then the radio stops. A failed write asks for a second press. After
 * a launch, one result line shows under the list until the selection
 * moves or the sub-tab is left.
 */
#include "model/util.h"
#include "screens/data_view.h"
#include "services/services.h"
#include "ui/frame.h"
#include "ui/list.h"

#define ICON_COL  40 /* the icon column at the right of the list */
#define ICON_SIZE 32
#define RESULT_H  18 /* the result row and its gap */
#define LOG_SIZE  128

typedef struct tapes_state {
    int ready;
    dx_holo_t holos[DX_HOLO_MAX];
    int32_t listed; /* the count, or a negative error */
    dx_list_t list;
    const char *result; /* the result line, or NULL */
    int armed;          /* a failed save was shown: the next SELECT runs */
    char status[32];    /* "Tape ended, status <n>." */
} tapes_state_t;

static tapes_state_t tapes;

static const char *kind_mark(int kind)
{
    switch (kind) {
    case DX_HOLO_GAME:
        return "GAME";
    case DX_HOLO_UTILITY:
        return "UTIL";
    case DX_HOLO_DATA:
        return "DATA";
    default:
        return "?";
    }
}

static const char *label(const dx_holo_t *h)
{
    return h->title[0] != '\0' ? h->title : h->file;
}

static void tape_row(void *user, int i, dx_row_t *out)
{
    (void)user;
    out->label = label(&tapes.holos[i]);
    out->value = kind_mark(tapes.holos[i].kind);
    out->flags = 0;
}

static void setup(void)
{
    int h = DX_BODY_H - 20 - DX_DATA_LIST_DY - RESULT_H; /* 20: the sub-tab row */

    if (!tapes.ready) {
        tapes.ready = 1;
        dx_list_init(&tapes.list, 0, h / DX_ROW_H, tape_row, NULL, "No holotapes on the card.");
    }
}

void dx_data_tapes_reset(void)
{
    dx_mem_set(&tapes, 0, sizeof(tapes));
}

/* Reads the listing again. The selection stays on the same file when
 * it is still there. */
static void read_list(void)
{
    char keep[DX_NAME83_SIZE];
    int i, sel;

    setup();
    keep[0] = '\0';
    if (tapes.list.count > 0) {
        dx_str_cut(keep, sizeof(keep), tapes.holos[tapes.list.sel].file,
                   dx_str_len(tapes.holos[tapes.list.sel].file));
    }
    sel = tapes.list.sel;
    tapes.listed = dx_plat_holo_list(tapes.holos, DX_HOLO_MAX);
    tapes.list.count = tapes.listed > 0 ? tapes.listed : 0;
    for (i = 0; i < tapes.list.count; i++) {
        if (dx_str_eq(tapes.holos[i].file, keep)) {
            sel = i;
        }
    }
    dx_list_select(&tapes.list, sel);
}

void dx_data_tapes_enter(void)
{
    read_list();
}

void dx_data_tapes_leave(void)
{
    tapes.result = NULL;
    tapes.armed = 0;
}

static void log_rc(const char *what, const char *file, int32_t rc)
{
    char line[LOG_SIZE];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: data ");
    dx_sb_str(&sb, what);
    if (file != NULL) {
        dx_sb_str(&sb, file);
    } else {
        dx_sb_int(&sb, rc);
    }
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
}

static const char *result_text(int32_t rc, int kind)
{
    dx_sb_t sb;

    if (rc >= 0) {
        dx_sb_init(&sb, tapes.status, sizeof(tapes.status));
        dx_sb_str(&sb, "Tape ended, status ");
        dx_sb_int(&sb, rc);
        dx_sb_char(&sb, '.');
        return tapes.status;
    }
    switch (rc) {
    case DX_EFAULT:
        return "Tape was stopped by the system.";
    case DX_ENOEXEC:
        /* Pip-OS refuses a data tape this way: say why. */
        return kind == DX_HOLO_DATA ? "Data tapes cannot be started." : "Not a holotape.";
    case DX_EIO:
        return "Card error.";
    case DX_ENOENT:
        return "Tape not found.";
    default:
        return "The tape could not start.";
    }
}

static unsigned launch(void)
{
    const dx_holo_t *h = &tapes.holos[tapes.list.sel];
    char file[DX_NAME83_SIZE];
    int kind = h->kind;
    int32_t rc;

    if (kind == DX_HOLO_DATA) {
        tapes.result = result_text(DX_ENOEXEC, kind);
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    dx_str_cut(file, sizeof(file), h->file, dx_str_len(h->file));
    /* Save first. A failed write stops the first press: the user learns
     * the changes are not safe. A second press runs the tape anyway, so a
     * bad card never blocks every tape. */
    if (dx_autosave_flush(dx_plat_ticks()) != 0 && !tapes.armed) {
        tapes.armed = 1;
        tapes.result = "Save failed. SELECT again runs it.";
        return DX_UI_HANDLED | DX_UI_DIRTY;
    }
    tapes.armed = 0;
    /* Pip-OS stops the sound when a tape starts: the radio must know. */
    dx_radio_stop();
    log_rc("launch ", file, 0);
    rc = dx_plat_holo_launch(file, "");
    log_rc("tape rc=", NULL, rc);
    tapes.result = result_text(rc, kind);
    read_list();
    /* The back buffer holds what the tape drew: the frame redraws all. */
    dx_data_log_reset();
    return DX_UI_HANDLED | DX_UI_DIRTY;
}

unsigned dx_data_tapes_event(const dx_ui_event_t *ev)
{
    unsigned r;

    setup();
    if (ev->type == DX_UI_SELECT) {
        /* One launch per press. A SELECT still held when a tape returns
         * sends only repeats: Pip-OS empties the queue at the tape's end. */
        return tapes.list.count > 0 && !ev->repeat ? launch() : DX_UI_IGNORED;
    }
    r = dx_list_event(&tapes.list, ev);
    if (r != DX_UI_IGNORED) {
        tapes.result = NULL;
        tapes.armed = 0;
    }
    return r;
}

void dx_data_tapes_draw(const dx_ctx_t *ctx, dx_rect_t r)
{
    const dx_holo_t *h;
    char line[LOG_SIZE];
    dx_sb_t sb;

    setup();
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: data tapes sel=");
    dx_sb_int(&sb, tapes.list.sel);
    dx_sb_str(&sb, " top=");
    dx_sb_int(&sb, tapes.list.top);
    dx_sb_str(&sb, " count=");
    dx_sb_int(&sb, tapes.listed);
    dx_sb_str(&sb, " | ");
    dx_sb_str(&sb, tapes.result != NULL ? tapes.result : "");
    dx_sb_char(&sb, '\n');
    dx_data_log(line);
    if (tapes.result != NULL) {
        ctx->gfx->text(r.x + 4, r.y - DX_GRID_BAND_H + DX_GRID_STATUS_DY, tapes.result,
                       ctx->theme.dim,
                       DX_COLOR_NONE, 1);
    }
    if (tapes.listed < 0) {
        dx_data_message(ctx, r, "The holotapes could not be read.", NULL);
        return;
    }
    if (tapes.list.count == 0) {
        /* Centred like the empty notes list. */
        dx_list_draw(&tapes.list, ctx,
                     (dx_rect_t){ r.x, r.y + DX_DATA_LIST_DY, r.w, r.h - DX_DATA_LIST_DY });
        return;
    }
    dx_list_draw(&tapes.list, ctx,
                 (dx_rect_t){ r.x, r.y + DX_DATA_LIST_DY, r.w - ICON_COL,
                              r.h - DX_DATA_LIST_DY - RESULT_H });
    h = &tapes.holos[tapes.list.sel];
    if (h->has_icon) {
        ctx->gfx->blit1(r.x + r.w - 4 - ICON_SIZE, r.y + DX_DATA_LIST_DY, ICON_SIZE, ICON_SIZE,
                        h->icon, ICON_SIZE / 8, ctx->theme.fg, DX_COLOR_NONE);
    }
}

const char *dx_data_tapes_hint(void)
{
    setup();
    return tapes.list.count > 0 ? "UP/DOWN: move  SELECT: run tape" : DX_HINT_PAGE;
}
