/* The footer notice queue: see notice.h. */
#include "ui/notice.h"

#include "model/util.h"
#include "ui/ui.h"

void dx_notice_init(dx_notice_q_t *q)
{
    dx_mem_set(q, 0, sizeof(*q));
}

void dx_notice_clear(dx_notice_q_t *q)
{
    dx_notice_init(q);
}

static void drop(dx_notice_q_t *q, int i)
{
    for (; i + 1 < q->count; i++) {
        q->text[i] = q->text[i + 1];
    }
    q->count--;
}

void dx_notice_push(dx_notice_q_t *q, const char *text)
{
    if (text == NULL) {
        return;
    }
    if (q->count > 0 && q->text[q->count - 1] == text) {
        return;
    }
    if (q->count == DX_NOTICE_MAX) {
        /* The one shown keeps its time: drop the oldest that waits. */
        drop(q, q->started ? 1 : 0);
    }
    q->text[q->count++] = text;
}

int dx_notice_tick(dx_notice_q_t *q, uint32_t now_ms)
{
    int changed = 0;

    if (q->started && dx_ticks_since(q->since, now_ms) >= DX_NOTICE_MS) {
        drop(q, 0);
        q->started = 0;
        changed = 1;
    }
    if (!q->started && q->count > 0) {
        q->started = 1;
        q->since = now_ms;
        changed = 1;
    }
    return changed;
}

const char *dx_notice_current(const dx_notice_q_t *q)
{
    return q->started ? q->text[0] : NULL;
}

uint32_t dx_notice_wake(const dx_notice_q_t *q)
{
    return q->started ? dx_ticks_add(q->since, DX_NOTICE_MS) : 0;
}
