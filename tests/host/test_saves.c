/* DEITRIX.SAV and INV.SAV: the byte layouts, round trips and corruption. */
#include "rc_test.h"

#include <stdio.h>
#include <string.h>

#include "model/saves.h"
#include "platform/platform.h"

static dx_items_t it;

static void hex(const uint8_t *b, int n, char *out)
{
    int i;

    static const char digits[] = "0123456789abcdef";

    for (i = 0; i < n; i++) {
        out[2 * i] = digits[b[i] >> 4];
        out[2 * i + 1] = digits[b[i] & 15];
    }
    out[2 * n] = '\0';
}

static void load_items(const char *t)
{
    dx_items_load(&it, (const uint8_t *)t, (int32_t)strlen(t));
}

/* The layout of docs/CONTENT.md, pinned against Python's zlib.crc32. */
TEST(settings_layout)
{
    dx_settings_t s;
    uint8_t b[DX_SETTINGS_SIZE];
    char h[2 * DX_SETTINGS_SIZE + 1];

    dx_settings_defaults(&s, 70);
    s.last_tab = 4;
    s.volume = 55;
    s.ui_sound = 0;
    s.last_station = 2;
    dx_settings_encode(&s, b);
    hex(b, sizeof(b), h);
    CHECK_STR(h, "44585354010001002f196cf60437000200000000");
}

TEST(inv_layout)
{
    static uint8_t b[DX_INV_SAVE_MAX];
    char h[2 * DX_INV_SAVE_MAX + 1];
    int n;

    load_items("version = 1\n[categories]\nm = MISC\n[item.knife]\nname = K\ncat = m\n"
               "[item.abcdefghijkl]\nname = L\ncat = m\n");
    dx_items_set_count(&it, 0, 3);
    dx_items_toggle_marked(&it, 0);
    dx_items_set_count(&it, 1, 999);
    n = dx_inv_encode(&it, b);
    CHECK_EQ(n, 44);
    hex(b, n, h);
    CHECK_STR(h, "4458495601000200a00b41fd6b6e69666500000000000000030001006162636465666768"
                 "696a6b6ce7030000");
    /* No items: a header alone, the CRC of no bytes is 0. */
    load_items("version = 1\n");
    n = dx_inv_encode(&it, b);
    CHECK_EQ(n, 12);
    hex(b, n, h);
    CHECK_STR(h, "445849560100000000000000");
}

static uint32_t seed = 12345;

static uint32_t rnd(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 8;
}

TEST(settings_round_trip)
{
    int i;

    for (i = 0; i < 1000; i++) {
        dx_settings_t a, b;
        uint8_t buf[DX_SETTINGS_SIZE];

        dx_settings_defaults(&a, 0);
        a.last_tab = (uint8_t)(rnd() % 5);
        a.volume = (uint8_t)(rnd() % 101);
        a.ui_sound = (uint8_t)(rnd() % 2);
        a.last_station = (uint8_t)(rnd() % 7);
        dx_settings_encode(&a, buf);
        CHECK_EQ(dx_settings_decode(&b, buf, sizeof(buf), 33), DX_SAVE_OK);
        CHECK(a.last_tab == b.last_tab && a.volume == b.volume && a.ui_sound == b.ui_sound &&
              a.last_station == b.last_station);
    }
}

TEST(inv_round_trip)
{
    static uint8_t buf[DX_INV_SAVE_MAX];
    static dx_items_t copy;
    static char text[8192];
    int round, i, n, applied;

    for (round = 0; round < 50; round++) {
        int items = (int)(rnd() % 65);
        int len = snprintf(text, sizeof(text), "version = 1\n[categories]\nm = MISC\n");

        for (i = 0; i < items; i++) {
            /* Ids of 1 to 12 characters. */
            len += snprintf(text + len, sizeof(text) - (size_t)len,
                            "[item.%.*s%d]\nname = T\ncat = m\ncount = %u\n",
                            (int)(rnd() % 10), "abcdefghij", i, rnd() % 1000);
        }
        load_items(text);
        CHECK_EQ(it.item_count, items);
        for (i = 0; i < items; i++) {
            dx_items_set_count(&it, i, (int32_t)(rnd() % 1000));
            if (rnd() % 2) {
                dx_items_toggle_marked(&it, i);
            }
        }
        n = dx_inv_encode(&it, buf);
        CHECK_EQ(n, 12 + 16 * items);
        copy = it;
        CHECK_EQ(dx_inv_decode(&copy, buf, n, &applied), DX_SAVE_OK);
        CHECK_EQ(applied, items);
        for (i = 0; i < items; i++) {
            CHECK_EQ(copy.items[i].count, it.items[i].count);
            CHECK_EQ(copy.items[i].marked, it.items[i].marked);
        }
    }
}

TEST(settings_corruption)
{
    dx_settings_t a, b;
    uint8_t good[DX_SETTINGS_SIZE], bad[DX_SETTINGS_SIZE];
    int i, bit;

    dx_settings_defaults(&a, 50);
    a.last_tab = 3;
    a.volume = 20;
    a.ui_sound = 0;
    a.last_station = 5;
    dx_settings_encode(&a, good);
    /* Every bit flipped: the save is refused, or it decodes to values in range. */
    for (i = 0; i < DX_SETTINGS_SIZE; i++) {
        for (bit = 0; bit < 8; bit++) {
            int st;

            memcpy(bad, good, sizeof(bad));
            bad[i] ^= (uint8_t)(1u << bit);
            st = dx_settings_decode(&b, bad, sizeof(bad), 50);
            CHECK(st != DX_SAVE_OK);
            CHECK(b.last_tab == 0 && b.volume == 50 && b.ui_sound == 1 && b.last_station == 0);
        }
    }
    /* Each size other than 20 is refused. */
    for (i = 0; i < DX_SETTINGS_SIZE; i++) {
        CHECK_EQ(dx_settings_decode(&b, good, i, 50), DX_SAVE_BAD_SIZE);
    }
    CHECK_EQ(dx_settings_decode(&b, NULL, DX_FILE_MISSING, 50), DX_SAVE_MISSING);
    CHECK_EQ(dx_settings_decode(&b, NULL, DX_FILE_ERROR, 50), DX_SAVE_READ_ERROR);
}

/* A field out of range with a good CRC: that field takes its default. */
TEST(settings_fields_out_of_range)
{
    dx_settings_t a, b;
    uint8_t buf[DX_SETTINGS_SIZE];
    uint32_t crc;

    dx_settings_defaults(&a, 50);
    a.last_tab = 2;
    a.volume = 7;
    dx_settings_encode(&a, buf);
    buf[12] = 5;   /* tab */
    buf[14] = 2;   /* ui_sound */
    buf[15] = 7;   /* station */
    buf[16] = 9;   /* reserved: ignored */
    crc = dx_crc32(buf + 12, 8);
    buf[8] = (uint8_t)crc;
    buf[9] = (uint8_t)(crc >> 8);
    buf[10] = (uint8_t)(crc >> 16);
    buf[11] = (uint8_t)(crc >> 24);
    CHECK_EQ(dx_settings_decode(&b, buf, sizeof(buf), 50), DX_SAVE_OK);
    CHECK_EQ(b.last_tab, 0);
    CHECK_EQ(b.volume, 7);
    CHECK_EQ(b.ui_sound, 1);
    CHECK_EQ(b.last_station, 0);
    buf[13] = 101;
    crc = dx_crc32(buf + 12, 8);
    buf[8] = (uint8_t)crc;
    buf[9] = (uint8_t)(crc >> 8);
    buf[10] = (uint8_t)(crc >> 16);
    buf[11] = (uint8_t)(crc >> 24);
    CHECK_EQ(dx_settings_decode(&b, buf, sizeof(buf), 50), DX_SAVE_OK);
    CHECK_EQ(b.volume, 50);
}

static void reseal(uint8_t *buf, int n)
{
    uint32_t crc = dx_crc32(buf + 12, (uint32_t)(n - 12));

    buf[8] = (uint8_t)crc;
    buf[9] = (uint8_t)(crc >> 8);
    buf[10] = (uint8_t)(crc >> 16);
    buf[11] = (uint8_t)(crc >> 24);
}

TEST(inv_corruption)
{
    static uint8_t good[DX_INV_SAVE_MAX], bad[DX_INV_SAVE_MAX];
    int n, i, bit, applied;

    load_items("version = 1\n[categories]\nm = MISC\n[item.a]\nname = A\ncat = m\ncount = 4\n"
               "[item.b]\nname = B\ncat = m\n");
    dx_items_set_count(&it, 0, 9);
    dx_items_toggle_marked(&it, 1);
    n = dx_inv_encode(&it, good);
    for (i = 0; i < n; i++) {
        for (bit = 0; bit < 8; bit++) {
            memcpy(bad, good, (size_t)n);
            bad[i] ^= (uint8_t)(1u << bit);
            CHECK(dx_inv_decode(&it, bad, n, &applied) != DX_SAVE_OK);
            /* Refused: every item back to its ITEMS.INI default. */
            CHECK(it.items[0].count == 4 && it.items[0].marked == 0);
            CHECK(it.items[1].count == 1 && it.items[1].marked == 0);
        }
    }
    CHECK_EQ(dx_inv_decode(&it, good, n - 1, &applied), DX_SAVE_BAD_SIZE);
    CHECK_EQ(dx_inv_decode(&it, good, 5, &applied), DX_SAVE_BAD_SIZE);
    CHECK_EQ(dx_inv_decode(&it, NULL, DX_FILE_MISSING, &applied), DX_SAVE_MISSING);
    CHECK_EQ(dx_inv_decode(&it, good, n, &applied), DX_SAVE_OK);
    CHECK_EQ(it.items[0].count, 9);
    CHECK_EQ(it.items[1].marked, 1);
    /* count above 64 with a matching size is refused. */
    memset(bad, 0, sizeof(bad));
    memcpy(bad, "DXIV\x01\x00\x41\x00", 8);
    CHECK_EQ(dx_inv_decode(&it, bad, 12 + 16 * 65, &applied), DX_SAVE_BAD_SIZE);
}

TEST(inv_records)
{
    static uint8_t buf[DX_INV_SAVE_MAX];
    int applied;
    int n;

    /* Records: gone, a, a again, bad count for b, bad flags, id not padded. */
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "DXIV\x01\x00\x06\x00", 8);
    memcpy(buf + 12, "gone", 4);
    buf[12 + 12] = 5;
    memcpy(buf + 28, "a", 1);
    buf[28 + 12] = 7;
    buf[28 + 14] = 1;
    memcpy(buf + 44, "a", 1);
    buf[44 + 12] = 8;
    memcpy(buf + 60, "b", 1);
    buf[60 + 12] = 0xe8; /* 1000 */
    buf[60 + 13] = 0x03;
    buf[60 + 14] = 1;
    memcpy(buf + 76, "c", 1);
    buf[76 + 12] = 2;
    buf[76 + 14] = 3;    /* other bits set: flags default */
    memcpy(buf + 92, "d\0x", 3);
    buf[92 + 12] = 2;
    n = 12 + 16 * 6;
    reseal(buf, n);
    load_items("version = 1\n[categories]\nm = MISC\n[item.a]\nname = A\ncat = m\n"
               "[item.b]\nname = B\ncat = m\ncount = 6\n[item.c]\nname = C\ncat = m\n"
               "[item.d]\nname = D\ncat = m\n[item.e]\nname = E\ncat = m\ncount = 3\n");
    CHECK_EQ(dx_inv_decode(&it, buf, n, &applied), DX_SAVE_OK);
    CHECK_EQ(applied, 3);
    CHECK_EQ(it.items[0].count, 7); /* the first a wins */
    CHECK_EQ(it.items[0].marked, 1);
    CHECK_EQ(it.items[1].count, 6); /* count out of range: default */
    CHECK_EQ(it.items[1].marked, 1);
    CHECK_EQ(it.items[2].count, 2);
    CHECK_EQ(it.items[2].marked, 0);
    CHECK_EQ(it.items[3].count, 1); /* id not padded with NUL: dropped */
    CHECK_EQ(it.items[4].count, 3); /* no record: the ITEMS.INI count */
    CHECK_EQ(it.changes, 0);
    /* The next write leaves the dropped records out. */
    CHECK_EQ(dx_inv_encode(&it, buf), 12 + 16 * 5);
}

TEST(setters)
{
    dx_settings_t s;

    dx_settings_defaults(&s, 120);
    CHECK_EQ(s.volume, 100);
    s.changes = 0;
    CHECK_EQ(dx_settings_set_tab(&s, 0), 0);
    CHECK_EQ(dx_settings_set_tab(&s, 5), 0);
    CHECK_EQ(dx_settings_set_tab(&s, 4), 1);
    CHECK_EQ(dx_settings_set_volume(&s, 101), 0);
    CHECK_EQ(dx_settings_set_volume(&s, -1), 0);
    CHECK_EQ(dx_settings_set_volume(&s, 0), 1);
    CHECK_EQ(dx_settings_set_ui_sound(&s, 2), 0);
    CHECK_EQ(dx_settings_set_ui_sound(&s, 0), 1);
    CHECK_EQ(dx_settings_set_station(&s, 7), 0);
    CHECK_EQ(dx_settings_set_station(&s, 6), 1);
    CHECK_EQ(s.changes, 4);
    CHECK_STR(dx_save_reason(DX_SAVE_BAD_CRC), "ignored: bad crc");
}
