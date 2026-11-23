/*
 * The autosave service: it watches the change counters of the settings
 * and the items, and writes a save once the changes stop for
 * DX_SAVE_QUIET_MS, or DX_SAVE_MAX_MS after the first one.
 */
#include "services/services.h"

#include "platform/platform.h"

enum { SAVE_SETTINGS = 1, SAVE_INV = 2 };

static dx_settings_t *settings;
static dx_items_t *items;
static uint32_t seen_settings, seen_items;
static unsigned dirty;     /* SAVE_* bits */
static uint32_t first_ms;  /* the first change since the last write */
static uint32_t last_ms;   /* the latest change */
static int failures;       /* failed attempts in a row */
static uint32_t retry_ms;  /* the next attempt after a failure */
static uint8_t buf[DX_INV_SAVE_MAX];

void dx_autosave_attach(dx_settings_t *s, dx_items_t *it)
{
    settings = s;
    items = it;
}

int dx_autosave_pending(void)
{
    return dirty != 0;
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
    if (found == 0) {
        return;
    }
    if (dirty == 0) {
        first_ms = now_ms;
        failures = 0;
    }
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

static void write_dirty(void)
{
    if (dirty & SAVE_SETTINGS) {
        dx_settings_encode(settings, buf);
        if (write_one(DX_SETTINGS_NAME, "DEITRIX.SAV", DX_SETTINGS_SIZE)) {
            dirty &= ~(unsigned)SAVE_SETTINGS;
        }
    }
    if (dirty & SAVE_INV) {
        int n = dx_inv_encode(items, buf);

        if (write_one(DX_INV_NAME, "INV.SAV", (uint32_t)n)) {
            dirty &= ~(unsigned)SAVE_INV;
        }
    }
}

/* The wake time, or 0 for none, after a write attempt or a change. */
static uint32_t run(uint32_t now_ms, int force)
{
    int due;

    note_changes(now_ms);
    if (dirty == 0) {
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
    write_dirty();
    if (dirty == 0) {
        failures = 0;
        return 0;
    }
    if (++failures > DX_SAVE_RETRIES) {
        /* Give up until the next change: the UI never waits on the card. */
        dx_plat_log("deitrix: save gave up\n");
        dirty = 0;
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

static uint32_t tick(uint32_t now_ms)
{
    return run(now_ms, 0);
}

/* What the model holds at start is what the saves hold: no write. */
static void init(uint32_t now_ms)
{
    (void)now_ms;
    seen_settings = settings != NULL ? settings->changes : 0;
    seen_items = items != NULL ? items->changes : 0;
    dirty = 0;
    failures = 0;
}

static void stop(void)
{
    dx_autosave_flush(dx_plat_ticks());
}

const dx_service_t dx_service_autosave = { "autosave", init, tick, stop };
