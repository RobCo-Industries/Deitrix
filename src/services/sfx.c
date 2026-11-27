/*
 * Sound effects that are samples, not UI tones. The weep is a 140 ms
 * chirp at 37500 Hz from src/services/weep_table.c. It needs
 * sample_rate, new in ABI 1.5, and an output that rests at its
 * midpoint, or the play would thump at both ends.
 */
#include "services/services.h"

#include "platform/platform.h"
#include "services/weep_table.h"
#include "ui/ui.h"

_Static_assert(DX_WEEP_RATE == DX_RATE_HI, "the weep's rate");

int dx_sfx_weep_allowed(int ui_sound_on, int radio_on, uint32_t abi_version, uint32_t hw_flags)
{
    const uint32_t need = DX_HW_AUDIO | DX_HW_AUDIO_MID;

    return ui_sound_on && !radio_on && abi_version >= DX_ABI_1_5 && (hw_flags & need) == need;
}

int dx_sfx_weep(void)
{
    dx_sysinfo_t si;
    int32_t prev;
    int32_t rc;

    dx_plat_sysinfo(&si); /* zeroed on failure: no ABI, no flags */
    if (!dx_sfx_weep_allowed(dx_ui_sound_enabled(), dx_radio_state() != DX_RADIO_OFF,
                             si.abi_version, si.hw_flags)) {
        return 0;
    }
    prev = dx_plat_sample_rate(DX_WEEP_RATE);
    if (prev < 0) {
        return 0;
    }
    rc = dx_plat_sample(dx_weep, DX_WEEP_LEN);
    /* The play keeps its rate: the rate before comes back at once. */
    dx_plat_sample_rate((uint32_t)prev);
    return rc == 0;
}
