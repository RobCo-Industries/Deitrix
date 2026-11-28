/*
 * The autosave service: it watches the change counters of the settings
 * and the items, and writes a save once the changes stop for
 * DX_SAVE_QUIET_MS, or DX_SAVE_MAX_MS after the first one. From ABI
 * 1.6 it also stores the device's sound levels by the same rules, with
 * dx_plat_levels_store, once dx_autosave_note_levels counted a change.
 */
#include "services/services.h"

#include "platform/platform.h"

enum { SAVE_SETTINGS = 1, SAVE_INV = 2, SAVE_LEVELS = 4 };

static dx_settings_t *settings;
static dx_items_t *items;
static uint32_t seen_settings, seen_items;
static uint32_t level_changes, seen_levels; /* changes of the device's levels */
static unsigned dirty;     /* SAVE_* bits: not on the card */
static unsigned stalled;   /* dirty bits the retries gave up on */
static uint32_t first_ms;  /* the first change since the last write */
static uint32_t last_ms;   /* the latest change */
static int failures;       /* failed attempts in a row */
static uint32_t retry_ms;  /* the next attempt after a failure */
static uint32_t dirty_ms;  /* when dirty last turned on from 0 */
static int marked;         /* the header shows the unsaved mark */
static uint8_t buf[DX_INV_SAVE_MAX];

void dx_autosave_attach(dx_settings_t *s, dx_items_t *it)
{
    settings = s;
    items = it;
}

void dx_autosave_note_levels(void)
{
    dx_sysinfo_t si;

    if (dx_plat_sysinfo(&si) != 0 || si.abi_version < DX_ABI_1_6) {
        return; /* no stored levels before ABI 1.6 */
    }
    level_changes++;
}

int dx_autosave_pending(void)
{
    return (dirty & ~stalled) != 0;
}

int dx_autosave_unsaved(void)
{
    return dirty != 0;
}

int dx_autosave_marked(void)
{
    return marked;
}

static void note_changes(uint32_t now_ms)
{
    unsigned found = 0;

    if (settings != NULL && settings->changes != seen_settings) {
        seen_settings = settings->changes;
        found |= SAVE_SETTINGS;
    }
    if (items != NULL && items->changes != seen_items) {
        seen_items = items->changes;
        found |= SAVE_INV;
    }
    if (level_changes != seen_levels) {
        seen_levels = level_changes;
        found |= SAVE_LEVELS;
    }
    if (found == 0) {
        return;
    }
    /* A change to a save the retries gave up on starts them again. */
    if ((dirty & ~stalled) == 0 || (found & stalled) != 0) {
        first_ms = now_ms;
        failures = 0;
    }
    if (dirty == 0) {
        dirty_ms = now_ms;
    }
    stalled &= ~found;
    dirty |= found;
    last_ms = now_ms;
}

static int write_one(const char *name, const char *file, uint32_t size)
{
    int ok = dx_plat_save_write(name, buf, size) == 0;

    dx_plat_log("deitrix: save ");
    dx_plat_log(file);
    dx_plat_log(ok ? " ok\n" : " failed\n");
    return ok;
}

/* Tries each save in `which` that is not on the card. */
static void write_dirty(unsigned which)
{
    if (dirty & which & SAVE_SETTINGS) {
        dx_settings_encode(settings, buf);
        if (write_one(DX_SETTINGS_NAME, "DEITRIX.SAV", DX_SETTINGS_SIZE)) {
            dirty &= ~(unsigned)SAVE_SETTINGS;
            stalled &= ~(unsigned)SAVE_SETTINGS;
        }
    }
    if (dirty & which & SAVE_INV) {
        int n = dx_inv_encode(items, buf);

        if (write_one(DX_INV_NAME, "INV.SAV", (uint32_t)n)) {
            dirty &= ~(unsigned)SAVE_INV;
            stalled &= ~(unsigned)SAVE_INV;
        }
    }
    if (dirty & which & SAVE_LEVELS) {
        int32_t rc = dx_plat_levels_store();
        char line[48];
        dx_sb_t sb;

        dx_sb_init(&sb, line, sizeof(line));
        dx_sb_str(&sb, "deitrix: save PIPOS.SET ");
        if (rc == 0) {
            dx_sb_str(&sb, "ok");
            dirty &= ~(unsigned)SAVE_LEVELS;
            stalled &= ~(unsigned)SAVE_LEVELS;
        } else {
            dx_sb_str(&sb, "failed rc=");
            dx_sb_int(&sb, rc);
        }
        dx_sb_char(&sb, '\n');
        dx_plat_log(line);
    }
}

/* The wake time, or 0 for none, after a write attempt or a change.
 * force writes every save not on the card, stalled ones too. */
static uint32_t run(uint32_t now_ms, int force)
{
    int due;

    note_changes(now_ms);
    if ((dirty & ~stalled) == 0) {
        if (force && dirty != 0) {
            write_dirty(dirty); /* one attempt; the retries stay stopped */
        }
        return 0;
    }
    if (failures > 0) {
        due = force || dx_ticks_due(now_ms, retry_ms);
    } else {
        due = force || dx_ticks_since(last_ms, now_ms) >= DX_SAVE_QUIET_MS ||
              dx_ticks_since(first_ms, now_ms) >= DX_SAVE_MAX_MS;
    }
    if (!due) {
        if (failures > 0) {
            return retry_ms;
        }
        {
            uint32_t quiet = DX_SAVE_QUIET_MS - dx_ticks_since(last_ms, now_ms);
            uint32_t max = DX_SAVE_MAX_MS - dx_ticks_since(first_ms, now_ms);

            return dx_ticks_add(now_ms, quiet < max ? quiet : max);
        }
    }
    write_dirty(force ? dirty : dirty & ~stalled);
    if ((dirty & ~stalled) == 0) {
        failures = 0;
        return 0;
    }
    if (++failures > DX_SAVE_RETRIES) {
        /* Stop retrying until the next change or flush: the UI never
         * waits on the card. The changes stay marked as not saved. */
        dx_plat_log("deitrix: save gave up\n");
        stalled = dirty;
        failures = 0;
        return 0;
    }
    retry_ms = dx_ticks_add(now_ms, DX_SAVE_RETRY_MS);
    return retry_ms;
}

int dx_autosave_flush(uint32_t now_ms)
{
    run(now_ms, 1);
    return dirty != 0;
}

/* The earlier of two wake times, 0 meaning none. */
static uint32_t earlier(uint32_t now_ms, uint32_t a, uint32_t b)
{
    if (a == 0) {
        return b;
    }
    if (b == 0) {
        return a;
    }
    return dx_ticks_since(now_ms, a) <= dx_ticks_since(now_ms, b) ? a : b;
}

/* Writes as run() says, then follows the unsaved mark: on once changes
 * stay unsaved for DX_SAVE_MARK_MS, off once they reach the card. A
 * change of the mark asks for a redraw and logs a line. */
static uint32_t tick(uint32_t now_ms, unsigned *flags)
{
    uint32_t next = run(now_ms, 0);
    int late = dirty != 0 && dx_ticks_since(dirty_ms, now_ms) >= DX_SAVE_MARK_MS;

    if (late != marked) {
        marked = late;
        *flags |= DX_UI_DIRTY;
        dx_plat_log(late ? "deitrix: unsaved mark on\n" : "deitrix: unsaved mark off\n");
    }
    if (dirty != 0 && !late) {
        next = earlier(now_ms, next, dx_ticks_add(dirty_ms, DX_SAVE_MARK_MS));
    }
    return next;
}

/* What the model holds at start is what the saves hold: no write. */
static void init(uint32_t now_ms)
{
    (void)now_ms;
    seen_settings = settings != NULL ? settings->changes : 0;
    seen_items = items != NULL ? items->changes : 0;
    seen_levels = level_changes;
    dirty = 0;
    stalled = 0;
    failures = 0;
    marked = 0;
}

static void stop(void)
{
    dx_autosave_flush(dx_plat_ticks());
}

const dx_service_t dx_service_autosave = { "autosave", init, tick, stop };
