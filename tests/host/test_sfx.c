/* The weep: when it plays, and the order of its calls on the fake audio. */
#include "rc_test.h"

#include <string.h>

#include <robco/crc32.h>

#include "app.h"
#include "fake_platform.h"
#include "screens/screens.h"
#include "services/services.h"
#include "services/weep_table.h"
#include "ui/ui.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))

#define FLAGS_OK (DX_HW_AUDIO | DX_HW_AUDIO_MID)

/* A kernel that lets the weep play: ABI 1.5, audio resting mid. */
static void setup(void)
{
    fake_reset();
    dx_ui_sound_enable(1);
    dx_ui_sound_block(0);
    dx_radio_stop();
    fake.sysinfo.abi_version = DX_ABI_1_5;
    fake.sysinfo.hw_flags = FLAGS_OK | DX_HW_RTC;
}

TEST(weep_allowed_every_case)
{
    /* Every input on: it plays. */
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, FLAGS_OK), 1);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, 0x00010006u, FLAGS_OK | DX_HW_TAPE), 1);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, 0x00020000u, FLAGS_OK), 1);
    /* Each one off on its own: silent. */
    CHECK_INT(dx_sfx_weep_allowed(0, 0, DX_ABI_1_5, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 1, DX_ABI_1_5, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_4, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_3, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, 0, FLAGS_OK), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, DX_HW_AUDIO), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, DX_HW_AUDIO_MID), 0);
    CHECK_INT(dx_sfx_weep_allowed(1, 0, DX_ABI_1_5, 0), 0);
    /* Every input off. */
    CHECK_INT(dx_sfx_weep_allowed(0, 1, 0, 0), 0);
}

TEST(weep_sets_the_rate_plays_and_restores)
{
    setup();
    CHECK_INT(dx_sfx_weep(), 1);
    CHECK_INT(fake.audio_count, 3);
    CHECK_INT(fake.audio[0].op, FAKE_AU_RATE);
    CHECK_INT(fake.audio[0].n, 37500);
    CHECK_INT(fake.audio[1].op, FAKE_AU_SAMPLE);
    CHECK_INT(fake.audio[1].n, 5250);
    CHECK_INT(fake.audio[1].first, 128);
    CHECK_INT(fake.audio[2].op, FAKE_AU_RATE);
    CHECK_INT(fake.audio[2].n, 8000);
    /* The play took the high rate, and the rate is back to 8000. */
    CHECK_INT(fake.au_rate, 37500);
    CHECK_INT(fake.rate, 8000);
    CHECK_INT(fake.au_bytes, DX_WEEP_LEN);
    CHECK_INT(fake.au_crc, crc32(dx_weep, DX_WEEP_LEN));
}

TEST(weep_restores_whatever_rate_was_set)
{
    setup();
    fake.rate = DX_RATE_HI;
    CHECK_INT(dx_sfx_weep(), 1);
    CHECK_INT(fake.audio[2].n, 37500);
    CHECK_INT(fake.rate, 37500);
}

TEST(weep_table_shape)
{
    CHECK_INT(DX_WEEP_LEN, 5250);
    CHECK_INT(DX_WEEP_RATE, 37500);
    CHECK_INT(dx_weep[0], 128);
    CHECK_INT(dx_weep[DX_WEEP_LEN - 1], 128);
}

TEST(weep_silent_cases)
{
    /* UI sounds off. */
    setup();
    dx_ui_sound_enable(0);
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    dx_ui_sound_enable(1);
    /* An older kernel. */
    setup();
    fake.sysinfo.abi_version = DX_ABI_1_4;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* No audio driver. */
    setup();
    fake.sysinfo.hw_flags = DX_HW_AUDIO_MID;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* The output rests low. */
    setup();
    fake.sysinfo.hw_flags = DX_HW_AUDIO;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* sys_info fails. */
    setup();
    fake.sysinfo_fail = 1;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    /* sample_rate refuses: no sample at the wrong rate. */
    setup();
    fake.rate_result = DX_EINVAL;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
}

TEST(weep_silent_while_the_radio_plays)
{
    static const char ini[] = "version = 1\n[station.1]\nname = Tune\nkind = tune\nfile = T.TUN\n";
    static const char tune[] = "version 1\nC4 D4\n";
    static dx_radio_t radio;

    setup();
    dx_radio_load(&radio, (const uint8_t *)ini, (int32_t)strlen(ini));
    dx_radio_set_tune(&radio, 0, (const uint8_t *)tune, (int32_t)strlen(tune));
    dx_radio_attach(&radio);
    CHECK_INT(dx_radio_play(0, fake.ticks), 0);
    CHECK(dx_radio_state() != DX_RADIO_OFF);
    fake.audio_count = 0;
    CHECK_INT(dx_sfx_weep(), 0);
    CHECK_INT(fake.audio_count, 0);
    dx_radio_stop();
    CHECK_INT(dx_sfx_weep(), 1);
}

TEST(weep_once_after_the_first_frame)
{
    static dx_app_t app;
    const char *ready;

    setup();
    dx_app_init(&app, dx_rom_screens, dx_rom_screen_count);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 0);
    dx_app_step(&app);
    ready = strstr(fake.log, "deitrix: ready\n");
    CHECK(ready != NULL);
    CHECK_EQ(fake.presents, 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 1);
    CHECK_INT(fake_audio_ops(FAKE_AU_RATE), 2);
    /* Another frame plays nothing more. */
    app.dirty = 1;
    dx_app_step(&app);
    CHECK_EQ(fake.presents, 2);
    CHECK_INT(fake_audio_ops(FAKE_AU_SAMPLE), 1);
    dx_radio_stop();
}

TEST(no_weep_on_the_default_fake)
{
    static dx_app_t app;

    /* The fake's kernel reports nothing: the frame comes with no sound. */
    fake_reset();
    dx_app_init(&app, dx_rom_screens, dx_rom_screen_count);
    dx_app_step(&app);
    CHECK_INT(fake.audio_count, 0);
}
