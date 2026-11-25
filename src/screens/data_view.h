/*
 * The DATA tab's parts: data.c holds the sub-tabs and NOTES,
 * data_tapes.c the holotape launcher, data_clock.c the clock and its
 * setter. Each part has the same hooks; data.c calls the active one.
 */
#ifndef DX_SCREENS_DATA_VIEW_H
#define DX_SCREENS_DATA_VIEW_H

#include "ui/grid.h"
#include "ui/ui.h"

/* Offsets from the top of the area under the sub-tabs. */
#define DX_DATA_LIST_DY DX_GRID_GAP

/* Clears the whole tab, for the host tests. */
void dx_data_reset(void);

/* A centred dim message in r, and a second line when b is not NULL. */
void dx_data_message(const dx_ctx_t *ctx, dx_rect_t r, const char *a, const char *b);

/* Logs line once: again only after it changed or the log was reset. */
void dx_data_log(const char *line);

/* Forgets the last line: the next frame logs its line again. */
void dx_data_log_reset(void);

/* TAPES. */
void dx_data_tapes_reset(void);
void dx_data_tapes_enter(void); /* reads the listing */
void dx_data_tapes_leave(void); /* drops the result line */
unsigned dx_data_tapes_event(const dx_ui_event_t *ev);
void dx_data_tapes_draw(const dx_ctx_t *ctx, dx_rect_t r);
const char *dx_data_tapes_hint(void);

/* CLOCK. */
void dx_data_clock_reset(void);
void dx_data_clock_leave(void); /* closes the setter, drops the status line */
int dx_data_clock_nested(void); /* 1 while the setter is open */
unsigned dx_data_clock_event(const dx_ui_event_t *ev);
uint32_t dx_data_clock_tick(uint32_t now_ms, unsigned *flags);
void dx_data_clock_draw(const dx_ctx_t *ctx, dx_rect_t r);
const char *dx_data_clock_hint(void);

#endif
