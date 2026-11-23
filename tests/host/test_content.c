/* The whole card through the loaders: the sample content, a card
 * without data, the saves at start, and every broken tree of the
 * checker's test table. */
#include "rc_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "content.h"
#include "expect.h"
#include "fake_platform.h"

#define PCM_SIZE 256000

static void sample_card(void)
{
    fake_reset();
    CHECK(fake_load_tree("content", "") > 0);
    fake_add_fill("/PIPBOY/DATA/RADIO/HEARTH.PCM", PCM_SIZE, 128);
    fake_add_fill("/PIPBOY/DATA/RADIO/DRIFT.PCM", PCM_SIZE, 128);
}

static const char SAMPLE_LOG[] =
    "deitrix: load PROFILE.INI gauges=4 attrs=6 about=3\n"
    "deitrix: load ITEMS.INI items=14 categories=4\n"
    "deitrix: load NOTES notes=4\n"
    "deitrix: load MAP.INI maps=2 lines=16 places=12\n"
    "deitrix: load RADIO.INI stations=5\n"
    "deitrix: load RADIO/MORNING.TUN notes=%d ms=%d\n"
    "deitrix: load RADIO/WALK.TUN notes=%d ms=%d\n"
    "deitrix: load RADIO/LULLABY.TUN notes=%d ms=%d\n"
    "deitrix: load RADIO/HEARTH.PCM bytes=256000\n"
    "deitrix: load RADIO/DRIFT.PCM bytes=256000\n";

/* The note count and length the tool gives for a sample tune. */
static void tool_tune(const char *name, int *count, int *ms)
{
    size_t i;
    int k;

    *count = -1;
    *ms = 0;
    for (i = 0; i < sizeof(TUNES) / sizeof(TUNES[0]); i++) {
        if (strcmp(TUNES[i].name, name) == 0) {
            *count = TUNES[i].count;
            for (k = 0; k < TUNES[i].count; k++) {
                *ms += TUNE_NOTES[TUNES[i].first + k][1];
            }
        }
    }
}

TEST(sample_content)
{
    char want[1024];
    int c[3], ms[3];
    const dx_notes_t *n;

    sample_card();
    dx_content_load();
    tool_tune("MORNING.TUN", &c[0], &ms[0]);
    tool_tune("WALK.TUN", &c[1], &ms[1]);
    tool_tune("LULLABY.TUN", &c[2], &ms[2]);
    snprintf(want, sizeof(want), SAMPLE_LOG, c[0], ms[0], c[1], ms[1], c[2], ms[2]);
    CHECK_STR(fake.log, want);
    n = dx_content_notes();
    CHECK_STR(n->notes[0].file, "GARDEN01.TXT");
    CHECK_STR(n->notes[3].file, "WALK01.TXT");
    CHECK_STR(dx_content_profile()->name, "Field kit 7");
    CHECK_EQ(dx_content_radio()->stations[4].playable, 1);
    CHECK_EQ(fake.handles_open, 0);
}

TEST(sample_notes_listed_in_small_pages)
{
    sample_card();
    fake.dir_page = 1; /* dir_list gives one entry per call */
    dx_content_load();
    CHECK_EQ(dx_content_notes()->count, 4);
    CHECK_EQ(dx_content_radio()->stations[3].playable, 1);
}

TEST(note_body)
{
    char body[64];

    sample_card();
    dx_content_load();
    CHECK(dx_content_note_body(0, body, sizeof(body)) > 0);
    CHECK_EQ(dx_content_note_body(4, body, sizeof(body)), -1);
    fake_remove_file("/PIPBOY/DATA/NOTES/GARDEN01.TXT");
    CHECK_EQ(dx_content_note_body(0, body, sizeof(body)), -1);
}

TEST(no_data_folder)
{
    fake_reset();
    dx_content_load();
    CHECK_STR(fake.log, "deitrix: load PROFILE.INI missing\n"
                        "deitrix: load ITEMS.INI missing\n"
                        "deitrix: load NOTES missing\n"
                        "deitrix: load MAP.INI missing\n"
                        "deitrix: load RADIO.INI missing\n");
    CHECK_EQ(dx_content_items()->item_count, 0);
    CHECK_EQ(dx_content_notes()->count, 0);
    CHECK_EQ(dx_content_radio()->count, 0);
}

TEST(read_errors)
{
    sample_card();
    fake.read_fail = 1;
    dx_content_load();
    CHECK(strstr(fake.log, "deitrix: load PROFILE.INI ignored: read error\n") != NULL);
    CHECK(strstr(fake.log, "deitrix: load NOTES ignored: read error\n") != NULL);
}

TEST(saves_at_start)
{
    uint8_t set[DX_SETTINGS_SIZE];
    static uint8_t inv[DX_INV_SAVE_MAX];
    dx_settings_t s;
    dx_items_t *it;
    int n;

    sample_card();
    dx_content_load();
    fake.log[0] = '\0';
    dx_content_load_saves(70);
    CHECK_STR(fake.log, "deitrix: load DEITRIX.SAV missing tab=0 volume=70 sound=1 station=0\n"
                        "deitrix: load INV.SAV missing\n");
    CHECK_EQ(fake.volume, 70);
    /* Saves written before: they apply on top of the content. */
    dx_settings_defaults(&s, 0);
    s.last_tab = 3;
    s.volume = 25;
    s.ui_sound = 0;
    s.last_station = 4;
    dx_settings_encode(&s, set);
    fake_put_save("DEITRIX", set, sizeof(set));
    it = dx_content_items();
    dx_items_set_count(it, 1, 7);
    n = dx_inv_encode(it, inv);
    fake_put_save("INV", inv, (uint32_t)n);
    sample_card();
    fake_put_save("DEITRIX", set, sizeof(set));
    fake_put_save("INV", inv, (uint32_t)n);
    dx_content_load();
    fake.log[0] = '\0';
    dx_content_load_saves(70);
    CHECK_STR(fake.log, "deitrix: load DEITRIX.SAV ok tab=3 volume=25 sound=0 station=4\n"
                        "deitrix: load INV.SAV ok records=14\n");
    CHECK_EQ(dx_content_settings()->last_tab, 3);
    CHECK_EQ(dx_content_items()->items[1].count, 7);
    CHECK_EQ(fake.volume, 25);
    /* UI sounds are off as the save says. */
    dx_ui_sound(DX_SND_MOVE);
    CHECK_EQ(fake.tone_count, 0);
    dx_ui_sound_enable(1);
    /* A save longer than its layout is refused. */
    {
        uint8_t big[DX_SETTINGS_SIZE + 4];

        memcpy(big, set, sizeof(set));
        memset(big + sizeof(set), 0, 4);
        fake_put_save("DEITRIX", big, sizeof(big));
        fake.log[0] = '\0';
        dx_content_load_saves(70);
        CHECK(strstr(fake.log, "DEITRIX.SAV ignored: bad size") != NULL);
    }
}

/* ---- The broken trees of tests/tools/test_check_content.py -------------- */

/* The content log of the sample card: what each broken tree changes is
 * compared against it. */
static char base_log[FAKE_LOG_SIZE];

static void broken_card(const broken_case_t *c)
{
    int i;

    sample_card();
    for (i = 0; i < c->count; i++) {
        const broken_file_t *f = &c->files[i];

        if (f->data == NULL && f->size < 0) {
            fake_remove_file(f->path);
        } else if (f->data == NULL) {
            uint8_t *b = malloc((size_t)f->size);

            memset(b, f->first, (size_t)f->size);
            b[f->size - 1] = (uint8_t)f->last;
            fake_add_file(f->path, b, (uint32_t)f->size);
        } else {
            fake_add_file(f->path, f->data, (uint32_t)f->size);
        }
    }
}

/* Appends the lines of `a` that `b` does not have, each after `mark`. */
static void lines_not_in(const char *a, const char *b, const char *mark, char *out, size_t cap)
{
    while (*a != '\0') {
        const char *e = strchr(a, '\n');
        size_t n = e != NULL ? (size_t)(e - a + 1) : strlen(a);
        char line[256];

        snprintf(line, sizeof(line), "%.*s", (int)n, a);
        if (strstr(b, line) == NULL) {
            strncat(out, mark, cap - strlen(out) - 1);
            strncat(out, line, cap - strlen(out) - 1);
        }
        a += n;
    }
}

/* The log lines of this load that the sample's log does not have, then
 * the sample's lines that this load lost, marked "-". */
static void diff_lines(char *out, size_t cap)
{
    out[0] = '\0';
    lines_not_in(fake.log, base_log, "", out, cap);
    lines_not_in(base_log, fake.log, "-", out, cap);
}

/* What the ROM does with each tree: the log lines that change. "" when
 * the ROM loads the file as the sample, because the rule it breaks is
 * one the ROM repairs without a change in counts, or ignores. */
static const struct {
    const char *what;
    const char *lines;
} EXPECT[] = {
#include "test_content_expect.inc"
};

TEST(broken_trees)
{
    size_t i, k;
    char got[1024];

    sample_card();
    dx_content_load();
    snprintf(base_log, sizeof(base_log), "%s", fake.log);
    for (i = 0; i < sizeof(BROKEN) / sizeof(BROKEN[0]); i++) {
        const char *want = NULL;

        broken_card(&BROKEN[i]);
        dx_content_load();
        diff_lines(got, sizeof(got));
        for (k = 0; k < sizeof(EXPECT) / sizeof(EXPECT[0]); k++) {
            if (strcmp(EXPECT[k].what, BROKEN[i].what) == 0) {
                want = EXPECT[k].lines;
            }
        }
        if (want == NULL || strcmp(want, got) != 0) {
            fprintf(stderr, "    { \"%s\", \"", BROKEN[i].what);
            for (k = 0; got[k] != '\0'; k++) {
                fprintf(stderr, got[k] == '\n' ? "\\n" : "%c", got[k]);
            }
            fprintf(stderr, "\" },%s\n", want == NULL ? " /* no expectation */" : "");
            CHECK(0);
        }
        /* Whatever the tree, nothing stays open. */
        CHECK_EQ(fake.handles_open, 0);
    }
    CHECK(sizeof(BROKEN) / sizeof(BROKEN[0]) >= 80);
}
