/*
 * Sound effects that are samples, not UI tones. The weep is a 140 ms
 * chirp at 37500 Hz from src/services/weep_table.c. The other sounds
 * come from src/services/sfx_table.c, at the same rate. Each needs
 * sample_rate, new in ABI 1.5, and an output that rests at its
 * midpoint, or the play would thump at both ends.
 */
#include "services/services.h"

#include "platform/platform.h"
#include "services/sfx_table.h"
#include "services/weep_table.h"
#include "ui/ui.h"

_Static_assert(DX_WEEP_RATE == DX_RATE_HI, "the weep's rate");
_Static_assert(DX_SFX_RATE_HZ == DX_RATE_HI, "the tables' rate");

int dx_sfx_mid(uint32_t abi_version, uint32_t hw_flags)
{
    const uint32_t need = DX_HW_AUDIO | DX_HW_AUDIO_MID;

    return abi_version >= DX_ABI_1_5 && (hw_flags & need) == need;
}

int dx_sfx_weep_allowed(int ui_sound_on, int radio_on, uint32_t abi_version, uint32_t hw_flags)
{
    return ui_sound_on && !radio_on && dx_sfx_mid(abi_version, hw_flags);
}

int dx_sfx_full(void)
{
    dx_sysinfo_t si;

    dx_plat_sysinfo(&si); /* zeroed on failure: no ABI, no flags */
    return dx_sfx_mid(si.abi_version, si.hw_flags);
}

/* Plays len bytes of data at DX_RATE_HI, then sets the rate back to the
 * one before. The play keeps the rate it started with, so a radio track
 * that starts later gets its own rate. 1 when the play started. */
static int play_hi(const uint8_t *data, uint32_t len)
{
    int32_t prev;
    int32_t rc;

    prev = dx_plat_sample_rate(DX_RATE_HI);
    if (prev < 0) {
        return 0;
    }
    rc = dx_plat_sample(data, len);
    dx_plat_sample_rate((uint32_t)prev);
    return rc == 0;
}

int dx_sfx_play(int id)
{
    static const struct {
        const uint8_t *data;
        uint32_t len;
    } tables[DX_SFX_COUNT] = {
        [DX_SFX_ID_SCROLL] = { dx_sfx_scroll, DX_SFX_SCROLL_LEN },
        [DX_SFX_ID_SELECT] = { dx_sfx_select, DX_SFX_SELECT_LEN },
    };

    if (id < 0 || id >= DX_SFX_COUNT || dx_radio_state() != DX_RADIO_OFF || !dx_sfx_full()) {
        return 0;
    }
    return play_hi(tables[id].data, tables[id].len);
}

int dx_sfx_weep(void)
{
    dx_sysinfo_t si;

    dx_plat_sysinfo(&si); /* zeroed on failure: no ABI, no flags */
    if (!dx_sfx_weep_allowed(dx_ui_sound_enabled(), dx_radio_state() != DX_RADIO_OFF,
                             si.abi_version, si.hw_flags)) {
        return 0;
    }
    return play_hi(dx_weep, DX_WEEP_LEN);
}
