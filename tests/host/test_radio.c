/* RADIO.INI, station availability, and the .TUN parser against the tool. */
#include "rc_test.h"

#include <stdio.h>
#include <string.h>

#include "expect.h"
#include "fake_platform.h"
#include "model/radio.h"

static dx_radio_t r;
static char text[2048];

static void load(const char *t)
{
    dx_radio_load(&r, (const uint8_t *)t, (int32_t)strlen(t));
}

TEST(stations)
{
    load("version = 1\n[station.4]\nname = Hearth\nkind = pcm\nfile = HEARTH.PCM\nloop = no\n"
         "[station.1]\nname = Morning music for the kit\nkind = tune\nfile = MORNING.TUN\n"
         "loop = no\n[station.2]\nname = P\nkind = pcm\nfile = P.PCM\nloop = maybe\n");
    CHECK_EQ(r.load.status, DX_LOAD_OK);
    CHECK_EQ(r.count, 3);
    CHECK_EQ(r.stations[0].number, 1);
    CHECK_STR(r.stations[0].name, "Morning music for th");
    CHECK_EQ(r.stations[0].kind, DX_KIND_TUNE);
    CHECK_EQ(r.stations[0].loop, 1); /* loop on a tune: ignored */
    CHECK_EQ(r.stations[1].loop, 1); /* any other value: yes */
    CHECK_EQ(r.stations[2].number, 4);
    CHECK_EQ(r.stations[2].loop, 0);
    CHECK_STR(r.stations[2].file, "HEARTH.PCM");
    CHECK_EQ(r.stations[2].playable, 0); /* until its file is seen */
    CHECK_EQ(dx_radio_find(&r, 4), 2);
    CHECK_EQ(dx_radio_find(&r, 3), -1);
}

TEST(stations_dropped)
{
    static const char *const bad[] = {
        "kind = tune\nfile = A.TUN\n",               /* no name */
        "name =\nkind = tune\nfile = A.TUN\n",       /* empty name */
        "name = A\nfile = A.TUN\n",                  /* no kind */
        "name = A\nkind = midi\nfile = A.TUN\n",
        "name = A\nkind = tune\n",                   /* no file */
        "name = A\nkind = tune\nfile = A.PCM\n",     /* wrong extension */
        "name = A\nkind = pcm\nfile = A.TUN\n",
        "name = A\nkind = tune\nfile = a.tun\n",     /* not upper case */
        "name = A\nkind = tune\nfile = LONGNAME1.TUN\n",
        "name = A\nkind = tune\nfile = RADIO/A.TUN\n",
    };
    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        snprintf(text, sizeof(text), "version = 1\n[station.1]\n%s", bad[i]);
        load(text);
        CHECK_EQ(r.load.status, DX_LOAD_OK);
        CHECK_EQ(r.count, 0);
    }
    load("version = 1\n[station.7]\nname = A\nkind = tune\nfile = A.TUN\n"
         "[station.0]\nname = A\nkind = tune\nfile = A.TUN\n"
         "[station.10]\nname = A\nkind = tune\nfile = A.TUN\n");
    CHECK_EQ(r.count, 0);
    load("[station.1]\nname = A\nkind = tune\nfile = A.TUN\n");
    CHECK_EQ(r.load.status, DX_LOAD_VERSION);
    CHECK_EQ(r.count, 0);
}

TEST(availability)
{
    static const char tune[] = "version 1\nC4 D4\n";

    load("version = 1\n[station.1]\nname = T\nkind = tune\nfile = T.TUN\n"
         "[station.2]\nname = P\nkind = pcm\nfile = P.PCM\n");
    dx_radio_set_tune(&r, 0, (const uint8_t *)tune, (int32_t)strlen(tune));
    CHECK_EQ(r.stations[0].playable, 1);
    CHECK_EQ(r.stations[0].note_count, 2);
    CHECK_EQ(r.stations[0].tune_ms, 1000);
    dx_radio_set_tune(&r, 0, NULL, DX_FILE_MISSING);
    CHECK_EQ(r.stations[0].playable, 0);
    CHECK_EQ(r.stations[0].file_load.status, DX_LOAD_MISSING);
    dx_radio_set_tune(&r, 0, NULL, DX_FILE_TOO_LARGE);
    CHECK_EQ(r.stations[0].file_load.status, DX_LOAD_TOO_LARGE);
    dx_radio_set_tune(&r, 0, (const uint8_t *)"version 1\nX4\n", 13);
    CHECK_EQ(r.stations[0].playable, 0);
    CHECK_EQ(r.stations[0].file_load.status, DX_LOAD_BROKEN);
    CHECK_EQ(r.stations[0].file_load.line, 2);
    dx_radio_set_pcm(&r, 1, 8000);
    CHECK_EQ(r.stations[1].playable, 1);
    dx_radio_set_pcm(&r, 1, 2097152);
    CHECK_EQ(r.stations[1].playable, 1);
    dx_radio_set_pcm(&r, 1, 7999);
    CHECK_EQ(r.stations[1].playable, 0);
    CHECK_EQ(r.stations[1].file_load.status, DX_LOAD_BROKEN);
    dx_radio_set_pcm(&r, 1, 2097153);
    CHECK_EQ(r.stations[1].playable, 0);
    CHECK_EQ(r.stations[1].file_load.status, DX_LOAD_TOO_LARGE);
    dx_radio_set_pcm(&r, 1, DX_FILE_MISSING);
    CHECK_EQ(r.stations[1].file_load.status, DX_LOAD_MISSING);
    /* The wrong kind is left alone. */
    dx_radio_set_pcm(&r, 0, 9000);
    CHECK_EQ(r.stations[0].pcm_size, 0);
}

/* Every case of the generated table: the notes, note for note, or the
 * error line, as tools/tun2notes.py gives them. */
TEST(tunes_match_the_tool)
{
    static dx_note_t out[DX_TUN_NOTES_MAX];
    size_t i;
    int k;

    for (i = 0; i < sizeof(TUNES) / sizeof(TUNES[0]); i++) {
        const tune_case_t *c = &TUNES[i];
        int count = -1;
        uint32_t ms = 0;
        unsigned line = 99;
        int ok = dx_tun_parse((const uint8_t *)c->data, c->size, out, &count, &ms, &line);
        uint32_t sum = 0;

        if (ok != (c->err_line < 0) || (!ok && (int)line != c->err_line)) {
            fprintf(stderr, "  tune %s: ok %d line %u, the tool: %d\n", c->name, ok, line,
                    c->err_line);
            CHECK(0);
            continue;
        }
        if (!ok) {
            continue;
        }
        CHECK_EQ(count, c->count);
        for (k = 0; k < count && k < c->count; k++) {
            CHECK_EQ(out[k].freq_hz, TUNE_NOTES[c->first + k][0]);
            CHECK_EQ(out[k].duration_ms, TUNE_NOTES[c->first + k][1]);
            sum += out[k].duration_ms;
        }
        CHECK_EQ(ms, sum);
    }
    CHECK(sizeof(TUNES) / sizeof(TUNES[0]) > 40);
}

TEST(tune_size_limit)
{
    static uint8_t big[DX_TUN_LIMIT + 1];
    dx_note_t out[4];
    int count;
    uint32_t ms;
    unsigned line;

    memset(big, ' ', sizeof(big));
    CHECK_EQ(dx_tun_parse(big, (int32_t)sizeof(big), out, &count, &ms, &line), 0);
    CHECK_EQ(line, 0);
}

TEST(sample)
{
    static uint8_t buf[DX_TUN_LIMIT];
    int32_t n;
    int i;

    fake_reset();
    CHECK(fake_load_tree("content", "") > 0);
    n = dx_plat_read_file("/PIPBOY/DATA/RADIO.INI", buf, DX_RADIO_LIMIT);
    CHECK(n > 0);
    dx_radio_load(&r, buf, n);
    CHECK_EQ(r.load.status, DX_LOAD_OK);
    CHECK_EQ(r.count, 5);
    for (i = 0; i < 3; i++) {
        char path[64];

        snprintf(path, sizeof(path), "/PIPBOY/DATA/RADIO/%s", r.stations[i].file);
        n = dx_plat_read_file(path, buf, sizeof(buf));
        dx_radio_set_tune(&r, i, buf, n);
        CHECK_EQ(r.stations[i].playable, 1);
    }
    CHECK_EQ(r.stations[3].kind, DX_KIND_PCM);
    CHECK_EQ(r.stations[3].loop, 1);
}
