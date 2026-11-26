/*
 * The tape slot service, ABI 1.4. At start it reads hw_flags: with no
 * slot, or on a Pip-OS before 1.4, it does nothing at all. With a slot
 * it asks holo_eject_event for the changes on each tick. The shell
 * ticks it after every event, a PIP_EV_TAPE included, and TAPES asks
 * after each launch. Each change counts a generation the TAPES list
 * watches, and queues a notice for the footer.
 */
#include "services/services.h"

#include "model/util.h"
#include "platform/platform.h"

static int slot;           /* the kernel reports a slot */
static uint32_t gen;       /* changes seen */
static dx_notice_q_t notices;

int dx_tapes_slot(void)
{
    return slot;
}

uint32_t dx_tapes_gen(void)
{
    return gen;
}

const char *dx_tapes_notice(void)
{
    return dx_notice_current(&notices);
}

static void log_bits(uint32_t bits)
{
    char line[48];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: tape slot bits=");
    dx_sb_uint(&sb, bits);
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
}

static void log_notice(const char *text)
{
    char line[64];
    dx_sb_t sb;

    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: notice ");
    dx_sb_str(&sb, text != NULL ? text : "-");
    dx_sb_char(&sb, '\n');
    dx_plat_log(line);
}

/* Several changes between two calls lose their order: a removal goes
 * first, then what the slot holds now. */
static void take(uint32_t bits)
{
    if (bits == 0) {
        return;
    }
    gen++;
    log_bits(bits);
    if (bits & DX_HOLO_EV_REMOVED) {
        dx_notice_push(&notices, DX_TAPES_OUT);
    }
    if (bits & DX_HOLO_EV_INSERTED) {
        dx_notice_push(&notices, DX_TAPES_IN);
    }
    if (bits & DX_HOLO_EV_BAD) {
        dx_notice_push(&notices, DX_TAPES_BAD);
    }
}

/* Moves the notice queue on: 1 when the footer changed. */
static int advance(uint32_t now_ms)
{
    if (dx_notice_tick(&notices, now_ms)) {
        log_notice(dx_notice_current(&notices));
        return 1;
    }
    return 0;
}

uint32_t dx_tapes_poll(uint32_t now_ms)
{
    uint32_t bits = slot ? dx_plat_holo_events() : 0;

    take(bits);
    advance(now_ms);
    return bits;
}

void dx_tapes_reset(void)
{
    slot = 0;
    gen = 0;
    dx_notice_init(&notices);
}

static void init(uint32_t now_ms)
{
    dx_sysinfo_t si;

    (void)now_ms;
    dx_tapes_reset();
    /* The flag alone is not enough: before 1.3 the field was reserved,
     * and a slot needs the 1.4 calls. */
    slot = dx_plat_sysinfo(&si) == 0 && si.abi_version >= DX_ABI_1_4 &&
           (si.hw_flags & DX_HW_TAPE) != 0;
    dx_plat_log(slot ? "deitrix: tape slot on\n" : "deitrix: tape slot off\n");
}

static uint32_t tick(uint32_t now_ms, unsigned *flags)
{
    uint32_t before = gen;

    if (!slot) {
        return 0;
    }
    take(dx_plat_holo_events());
    if (advance(now_ms) || gen != before) {
        *flags |= DX_UI_DIRTY;
    }
    return dx_notice_wake(&notices);
}

const dx_service_t dx_service_tapes = { "tapes", init, tick, NULL };
