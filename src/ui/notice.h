/*
 * A queue of short notices for the footer. Each shows in place of the
 * key hint for DX_NOTICE_MS, then the next one shows. A notice never
 * takes a key: the keys work as the hidden hint says.
 */
#ifndef DX_UI_NOTICE_H
#define DX_UI_NOTICE_H

#include <stdint.h>

#define DX_NOTICE_MAX 4    /* notices that wait, the one shown included */
#define DX_NOTICE_MS  3000 /* how long one shows */

typedef struct dx_notice_q {
    const char *text[DX_NOTICE_MAX]; /* static strings, oldest first */
    int count;
    int started;    /* the first one shows since `since` */
    uint32_t since;
} dx_notice_q_t;

void dx_notice_init(dx_notice_q_t *q);

/* Adds text at the end. A full queue drops the oldest notice that does
 * not show yet, so the newest news always gets its turn. The same text
 * as the last one waiting is not added twice. */
void dx_notice_push(dx_notice_q_t *q, const char *text);

/* Drops the notices that showed for DX_NOTICE_MS by now_ms and starts
 * the next one at now_ms. Returns 1 when the notice shown changed. */
int dx_notice_tick(dx_notice_q_t *q, uint32_t now_ms);

/* The notice shown, or NULL. */
const char *dx_notice_current(const dx_notice_q_t *q);

/* The tick when the notice shown ends, or 0 when none shows. */
uint32_t dx_notice_wake(const dx_notice_q_t *q);

/* Drops every notice. */
void dx_notice_clear(dx_notice_q_t *q);

#endif
