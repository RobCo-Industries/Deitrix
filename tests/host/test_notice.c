/* The footer notice queue and the tape slot service against the fake. */
#include "rc_test.h"

#include <string.h>

#include "fake_platform.h"
#include "services/services.h"
#include "ui/notice.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

static const char *const A = "a", *const B = "b", *const C = "c", *const D = "d",
                         *const E = "e";

TEST(notice_empty)
{
    dx_notice_q_t q;

    dx_notice_init(&q);
    CHECK(dx_notice_current(&q) == NULL);
    CHECK_INT(dx_notice_wake(&q), 0);
    CHECK_INT(dx_notice_tick(&q, 100), 0);
    dx_notice_push(&q, NULL);
    CHECK_INT(q.count, 0);
}

TEST(notice_shows_in_order_for_its_time)
{
    dx_notice_q_t q;

    dx_notice_init(&q);
    dx_notice_push(&q, A);
    dx_notice_push(&q, B);
    /* Nothing shows before the first tick. */
    CHECK(dx_notice_current(&q) == NULL);
    CHECK_INT(dx_notice_tick(&q, 1000), 1);
    CHECK(dx_notice_current(&q) == A);
    CHECK_INT(dx_notice_wake(&q), 1000 + DX_NOTICE_MS);
    CHECK_INT(dx_notice_tick(&q, 1000 + DX_NOTICE_MS - 1), 0);
    CHECK(dx_notice_current(&q) == A);
    CHECK_INT(dx_notice_tick(&q, 1000 + DX_NOTICE_MS), 1);
    CHECK(dx_notice_current(&q) == B);
    CHECK_INT(dx_notice_wake(&q), 1000 + 2 * DX_NOTICE_MS);
    CHECK_INT(dx_notice_tick(&q, 1000 + 2 * DX_NOTICE_MS + 50), 1);
    CHECK(dx_notice_current(&q) == NULL);
    CHECK_INT(dx_notice_wake(&q), 0);
}

TEST(notice_across_the_tick_wrap)
{
    dx_notice_q_t q;
    uint32_t start = DX_TICKS_MASK - 1000;

    dx_notice_init(&q);
    dx_notice_push(&q, A);
    dx_notice_tick(&q, start);
    CHECK_INT(dx_notice_wake(&q), DX_NOTICE_MS - 1001);
    CHECK_INT(dx_notice_tick(&q, 500), 0);
    CHECK(dx_notice_current(&q) == A);
    CHECK_INT(dx_notice_tick(&q, DX_NOTICE_MS - 1001), 1);
    CHECK(dx_notice_current(&q) == NULL);
}

TEST(notice_full_queue_drops_the_oldest_waiting)
{
    dx_notice_q_t q;

    dx_notice_init(&q);
    dx_notice_push(&q, A);
    dx_notice_tick(&q, 0);
    dx_notice_push(&q, B);
    dx_notice_push(&q, C);
    dx_notice_push(&q, D);
    CHECK_INT(q.count, DX_NOTICE_MAX);
    /* A shows: B, the oldest that waits, goes. */
    dx_notice_push(&q, E);
    CHECK_INT(q.count, DX_NOTICE_MAX);
    CHECK(q.text[0] == A && q.text[1] == C && q.text[2] == D && q.text[3] == E);
    CHECK(dx_notice_current(&q) == A);
    /* Nothing shows yet: the oldest goes. */
    dx_notice_clear(&q);
    dx_notice_push(&q, A);
    dx_notice_push(&q, B);
    dx_notice_push(&q, C);
    dx_notice_push(&q, D);
    dx_notice_push(&q, E);
    CHECK(q.text[0] == B && q.text[3] == E);
}

TEST(notice_same_text_twice_in_a_row_once)
{
    dx_notice_q_t q;

    dx_notice_init(&q);
    dx_notice_push(&q, A);
    dx_notice_push(&q, A);
    dx_notice_push(&q, B);
    dx_notice_push(&q, A);
    CHECK_INT(q.count, 3);
}

/* ---- The tape slot service ---------------------------------------------- */

static unsigned flags;

static uint32_t tick(uint32_t now)
{
    flags = 0;
    return dx_service_tapes.tick(now, &flags);
}

static void start(uint32_t abi, uint32_t hw)
{
    fake_reset();
    fake.sysinfo.abi_version = abi;
    fake.sysinfo.hw_flags = hw;
    dx_service_tapes.init(0);
}

TEST(tapes_no_slot_does_nothing)
{
    /* No slot flag; a 1.3 kernel with the bit set by mistake; a 1.2
     * kernel; a sys_info that fails. */
    static const uint32_t cases[][2] = {
        { DX_ABI_1_4, DX_HW_RTC | DX_HW_AUDIO },
        { DX_ABI_1_3, DX_HW_TAPE },
        { 0x00010002, DX_HW_TAPE },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]) + 1; i++) {
        if (i < sizeof(cases) / sizeof(cases[0])) {
            start(cases[i][0], cases[i][1]);
        } else {
            start(DX_ABI_1_4, DX_HW_TAPE);
            fake.sysinfo_fail = 1;
            dx_service_tapes.init(0);
        }
        CHECK_INT(dx_tapes_slot(), 0);
        CHECK(strstr(fake.log, "deitrix: tape slot off\n") != NULL);
        fake.holo_events = DX_HOLO_EV_INSERTED;
        CHECK_INT(tick(10), 0);
        CHECK_INT(flags, 0);
        CHECK_INT(dx_tapes_poll(10), 0);
        CHECK_INT(fake.holo_event_calls, 0);
        CHECK_INT(dx_tapes_gen(), 0);
        CHECK(dx_tapes_notice() == NULL);
    }
}

TEST(tapes_each_event_has_its_notice)
{
    static const struct {
        uint32_t bits;
        const char *text;
    } table[] = {
        { DX_HOLO_EV_INSERTED, DX_TAPES_IN },
        { DX_HOLO_EV_REMOVED, DX_TAPES_OUT },
        { DX_HOLO_EV_BAD, DX_TAPES_BAD },
    };
    size_t i;

    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        start(DX_ABI_1_4, DX_HW_TAPE);
        CHECK_INT(dx_tapes_slot(), 1);
        CHECK(strstr(fake.log, "deitrix: tape slot on\n") != NULL);
        /* Nothing yet: no wake, no frame. */
        CHECK_INT(tick(100), 0);
        CHECK_INT(flags, 0);
        CHECK_INT(fake.holo_event_calls, 1);
        fake.holo_events = table[i].bits;
        CHECK_INT(tick(200), 200 + DX_NOTICE_MS);
        CHECK_INT(flags, DX_UI_DIRTY);
        CHECK_INT(dx_tapes_gen(), 1);
        CHECK_STR(dx_tapes_notice(), table[i].text);
        CHECK(strstr(fake.log, table[i].text) != NULL);
        /* It shows for its time, then goes with one more frame. */
        CHECK_INT(tick(200 + DX_NOTICE_MS - 1), 200 + DX_NOTICE_MS);
        CHECK_INT(flags, 0);
        CHECK_INT(tick(200 + DX_NOTICE_MS), 0);
        CHECK_INT(flags, DX_UI_DIRTY);
        CHECK(dx_tapes_notice() == NULL);
        CHECK(strstr(fake.log, "deitrix: notice -\n") != NULL);
    }
}

TEST(tapes_several_bits_removal_first)
{
    start(DX_ABI_1_4, DX_HW_TAPE);
    fake.holo_events = DX_HOLO_EV_INSERTED | DX_HOLO_EV_REMOVED | DX_HOLO_EV_BAD;
    tick(0);
    CHECK_INT(dx_tapes_gen(), 1);
    CHECK_STR(dx_tapes_notice(), DX_TAPES_OUT);
    tick(DX_NOTICE_MS);
    CHECK_STR(dx_tapes_notice(), DX_TAPES_IN);
    tick(2 * DX_NOTICE_MS);
    CHECK_STR(dx_tapes_notice(), DX_TAPES_BAD);
    tick(3 * DX_NOTICE_MS);
    CHECK(dx_tapes_notice() == NULL);
}

TEST(tapes_poll_after_a_launch)
{
    start(DX_ABI_1_4, DX_HW_TAPE);
    fake.holo_events = DX_HOLO_EV_REMOVED;
    CHECK_INT(dx_tapes_poll(50), DX_HOLO_EV_REMOVED);
    CHECK_INT(dx_tapes_gen(), 1);
    CHECK_STR(dx_tapes_notice(), DX_TAPES_OUT);
    CHECK_INT(dx_tapes_poll(60), 0);
    CHECK_INT(dx_tapes_gen(), 1);
}
