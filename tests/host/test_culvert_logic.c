/* Culvert's pure logic: pieces, the generator, the flood fill, the
 * solved test, the timer and the save record. */
#include "rc_test.h"

#include "logic.h"
#include "tfmt.h"

#define CHECK_INT(a, b) CHECK_EQ((int64_t)(a), (int64_t)(b))
#define LEVELS 200

TEST(culvert_rotations)
{
    uint8_t m;

    CHECK_INT(cv_rot_cw(CV_N), CV_E);
    CHECK_INT(cv_rot_cw(CV_E), CV_S);
    CHECK_INT(cv_rot_cw(CV_S), CV_W);
    CHECK_INT(cv_rot_cw(CV_W), CV_N);
    CHECK_INT(cv_rot_cw(CV_N | CV_E), CV_E | CV_S);
    CHECK_INT(cv_rot_cw(CV_W | CV_N), CV_N | CV_E);
    CHECK_INT(cv_rot_cw(CV_N | CV_S), CV_E | CV_W);
    CHECK_INT(cv_rot_cw(CV_N | CV_E | CV_S), CV_E | CV_S | CV_W);
    CHECK_INT(cv_rot_cw(15), 15);
    for (m = 0; m < 16; m++) {
        int q;

        CHECK_INT(cv_rot_ccw(cv_rot_cw(m)), m);
        CHECK_INT(cv_rot_n(m, 4), m);
        CHECK_INT(cv_rot_n(m, -1), cv_rot_ccw(m));
        CHECK_INT(cv_rot_n(m, 6), cv_rot_n(m, 2));
        CHECK_INT(cv_bits(cv_rot_cw(m)), cv_bits(m));
        CHECK_INT(cv_piece(cv_rot_cw(m)), cv_piece(m));
        for (q = 0; q < 4; q++) {
            CHECK_INT(cv_rot_n(m, cv_turns_to(m, cv_rot_n(m, q))), cv_rot_n(m, q));
        }
    }
    CHECK_INT(cv_piece(0), CV_PIECE_NONE);
    CHECK_INT(cv_piece(CV_W), CV_PIECE_CAP);
    CHECK_INT(cv_piece(CV_E | CV_W), CV_PIECE_STRAIGHT);
    CHECK_INT(cv_piece(CV_S | CV_W), CV_PIECE_ELBOW);
    CHECK_INT(cv_piece(CV_N | CV_S | CV_W), CV_PIECE_TEE);
    CHECK_INT(cv_piece(15), CV_PIECE_CROSS);
    CHECK_INT(cv_turns_to(CV_N | CV_S, CV_E | CV_W), 1);
    CHECK_INT(cv_turns_to(CV_N | CV_S, CV_N | CV_S), 0);
    CHECK_INT(cv_turns_to(CV_N, CV_E | CV_W), -1);
}

TEST(culvert_sizes_and_seeds)
{
    uint8_t w, h;

    cv_class_size(cv_class(1), &w, &h);
    CHECK_INT(w, 5);
    CHECK_INT(h, 4);
    CHECK_INT(cv_class(3), 0);
    CHECK_INT(cv_class(4), 1);
    CHECK_INT(cv_class(25), 8);
    CHECK_INT(cv_class(1000000), 8);
    cv_class_size(8, &w, &h);
    CHECK_INT(w, CV_MAX_W);
    CHECK_INT(h, CV_MAX_H);
    /* Pinned, so a level is the same puzzle on every device. */
    CHECK_INT(cv_seed(1), cv_seed(1));
    CHECK(cv_seed(1) != cv_seed(2));
    CHECK(cv_seed(0) != 0);
}

/* Every opening of the goal network meets its neighbour's, none points
 * at a wall, edges = tiles - 1 and the source reaches every tile. */
static void check_tree(const cv_level_t *lv)
{
    cv_level_t g = *lv;
    uint8_t conn[CV_MAX_TILES];
    int n = lv->w * lv->h;
    int openings = 0;
    int x, y;

    for (y = 0; y < lv->h; y++) {
        for (x = 0; x < lv->w; x++) {
            uint8_t m = lv->goal[y * lv->w + x];

            openings += cv_bits(m);
            CHECK(m != 0);
            if (m & CV_N) {
                CHECK(y > 0 && (lv->goal[(y - 1) * lv->w + x] & CV_S));
            }
            if (m & CV_S) {
                CHECK(y < lv->h - 1 && (lv->goal[(y + 1) * lv->w + x] & CV_N));
            }
            if (m & CV_W) {
                CHECK(x > 0 && (lv->goal[y * lv->w + x - 1] & CV_E));
            }
            if (m & CV_E) {
                CHECK(x < lv->w - 1 && (lv->goal[y * lv->w + x + 1] & CV_W));
            }
        }
    }
    CHECK_INT(openings / 2, n - 1);
    for (x = 0; x < CV_MAX_TILES; x++) {
        g.mask[x] = g.goal[x];
    }
    CHECK_INT(cv_flood(&g, conn), n);
    CHECK(cv_solved(&g));
}

TEST(culvert_generator_levels)
{
    static cv_level_t a, b, prev;
    uint32_t level;

    for (level = 1; level <= LEVELS; level++) {
        uint8_t w, h;
        int i;

        cv_generate(&a, level);
        cv_generate(&b, level);
        cv_class_size(cv_class(level), &w, &h);
        CHECK_INT(a.w, w);
        CHECK_INT(a.h, h);
        CHECK_INT(a.seed, cv_seed(level));
        CHECK_MEM(a.mask, b.mask, sizeof a.mask);
        CHECK_MEM(a.goal, b.goal, sizeof a.goal);
        check_tree(&a);
        CHECK(!cv_solved(&a));
        for (i = 0; i < a.w * a.h; i++) {
            CHECK(cv_turns_to(a.mask[i], a.goal[i]) >= 0);
        }
        if (level > 1) {
            CHECK(a.w != prev.w || a.h != prev.h ||
                  __builtin_memcmp(a.goal, prev.goal, sizeof a.goal) != 0 ||
                  __builtin_memcmp(a.mask, prev.mask, sizeof a.mask) != 0);
        }
        prev = a;
    }
}

/* Turns every tile to its goal, then checks one quarter turn of any
 * tile but a cross breaks the solution. */
TEST(culvert_solved_test)
{
    static cv_level_t lv;
    uint32_t level;

    for (level = 1; level <= LEVELS; level += 7) {
        int x, y;

        cv_generate(&lv, level);
        for (y = 0; y < lv.h; y++) {
            for (x = 0; x < lv.w; x++) {
                cv_turn(&lv, x, y, cv_turns_to(lv.mask[y * lv.w + x], lv.goal[y * lv.w + x]));
            }
        }
        CHECK(cv_solved(&lv));
        for (y = 0; y < lv.h; y++) {
            for (x = 0; x < lv.w; x++) {
                if (cv_piece(lv.mask[y * lv.w + x]) == CV_PIECE_CROSS) {
                    continue;
                }
                cv_turn(&lv, x, y, 1);
                CHECK(!cv_solved(&lv));
                cv_turn(&lv, x, y, -1);
            }
        }
        CHECK(cv_solved(&lv));
    }
}

/* A tiny grid by hand: connected but with an opening into a wall. */
TEST(culvert_leak_is_not_solved)
{
    cv_level_t lv;
    uint8_t conn[CV_MAX_TILES];
    int i;

    for (i = 0; i < CV_MAX_TILES; i++) {
        lv.mask[i] = 0;
        lv.goal[i] = 0;
    }
    lv.w = 2;
    lv.h = 1;
    lv.sx = 0;
    lv.sy = 0;
    lv.mask[0] = CV_E;
    lv.mask[1] = CV_W;
    CHECK(cv_solved(&lv));
    lv.mask[1] = CV_W | CV_E;           /* into the east wall */
    CHECK_INT(cv_flood(&lv, conn), 2);
    CHECK(!cv_solved(&lv));
    lv.mask[1] = CV_W | CV_N;           /* into the north wall */
    CHECK(!cv_solved(&lv));
    lv.mask[0] = CV_E | CV_S;
    lv.mask[1] = CV_W;
    CHECK(!cv_solved(&lv));
    lv.mask[0] = CV_N;
    CHECK_INT(cv_flood(&lv, conn), 1);
    CHECK(!cv_solved(&lv));
}

/* The reference: relax until nothing changes. */
static int slow_flood(const cv_level_t *lv, uint8_t conn[CV_MAX_TILES])
{
    int changed = 1;
    int count = 1;
    int i;

    for (i = 0; i < CV_MAX_TILES; i++) {
        conn[i] = 0;
    }
    conn[lv->sy * lv->w + lv->sx] = 1;
    while (changed) {
        int x, y;

        changed = 0;
        for (y = 0; y < lv->h; y++) {
            for (x = 0; x < lv->w; x++) {
                uint8_t m = lv->mask[y * lv->w + x];
                int t = y * lv->w + x;
                int u = -1;

                if (!conn[t]) {
                    continue;
                }
                if ((m & CV_E) && x + 1 < lv->w && (lv->mask[t + 1] & CV_W) && !conn[t + 1]) {
                    u = t + 1;
                } else if ((m & CV_W) && x > 0 && (lv->mask[t - 1] & CV_E) && !conn[t - 1]) {
                    u = t - 1;
                } else if ((m & CV_S) && y + 1 < lv->h && (lv->mask[t + lv->w] & CV_N) &&
                           !conn[t + lv->w]) {
                    u = t + lv->w;
                } else if ((m & CV_N) && y > 0 && (lv->mask[t - lv->w] & CV_S) &&
                           !conn[t - lv->w]) {
                    u = t - lv->w;
                }
                if (u >= 0) {
                    conn[u] = 1;
                    count++;
                    changed = 1;
                }
            }
        }
    }
    return count;
}

TEST(culvert_flood_against_reference)
{
    static cv_level_t lv;
    uint32_t level;
    uint32_t rng = 12345;

    for (level = 1; level <= LEVELS; level++) {
        uint8_t a[CV_MAX_TILES], b[CV_MAX_TILES];
        int k;

        cv_generate(&lv, level);
        for (k = 0; k < 20; k++) {
            int n = lv.w * lv.h;
            int t = (int)(cv_rng_next(&rng) % (uint32_t)n);

            CHECK_INT(cv_flood(&lv, a), slow_flood(&lv, b));
            CHECK_MEM(a, b, sizeof a);
            cv_turn(&lv, t % lv.w, t / lv.w, (int)(cv_rng_next(&rng) % 4u));
        }
    }
}

TEST(culvert_timer)
{
    char buf[9];

    CHECK_INT(cv_elapsed(100, 1100), 1000);
    CHECK_INT(cv_elapsed(0x7FFFFF00u, 0x00000100u), 0x200);
    CHECK_INT(cv_elapsed(0x7FFFFFF6u, 0), 10);
    cv_fmt_time(buf, 0);
    CHECK_STR(buf, "0:00");
    cv_fmt_time(buf, 59999);
    CHECK_STR(buf, "0:59");
    cv_fmt_time(buf, 61000);
    CHECK_STR(buf, "1:01");
    cv_fmt_time(buf, 600000);
    CHECK_STR(buf, "10:00");
    cv_fmt_time(buf, 0x7FFFFFFFu);
    CHECK_STR(buf, "999:59");
    cv_fmt_time(buf, 0xFFFFFFFFu);
    CHECK_STR(buf, "999:59");
}

TEST(culvert_numbers)
{
    char buf[11];

    CHECK_INT(cv_fmt_u32(buf, 0), 1);
    CHECK_STR(buf, "0");
    CHECK_INT(cv_fmt_u32(buf, 4294967295u), 10);
    CHECK_STR(buf, "4294967295");
}

static void sample_save(cv_save_t *s)
{
    int i;

    cv_save_fresh(s);
    s->highest = 12;
    for (i = 0; i < CV_CLASSES; i++) {
        s->best[i] = (uint16_t)(i * 257 + 3);
    }
    s->best[8] = CV_NO_BEST;
}

TEST(culvert_save_round_trip)
{
    static const uint8_t pinned[CV_SAVE_SIZE] = {
        'C', 'V', 'L', 'T', 1, 9, 12, 0,
    };
    uint8_t buf[CV_SAVE_SIZE];
    cv_save_t s, t;
    char hex[3 * CV_SAVE_SIZE + 1];
    int i;

    sample_save(&s);
    cv_save_encode(&s, buf);
    CHECK_MEM(buf, pinned, 8);
    for (i = 0; i < CV_SAVE_SIZE; i++) {
        tfmt(hex + 3 * i, 4, "%02x ", buf[i]);
    }
    CHECK_STR(hex, "43 56 4c 54 01 09 0c 00 03 00 04 01 05 02 06 03 07 04 08 05 09 06 0a 07 "
                   "ff ff 7b 3d 9f a1 ");
    CHECK_INT(cv_save_decode(&t, buf, sizeof buf), 0);
    CHECK_INT(t.highest, 12);
    CHECK_MEM(t.best, s.best, sizeof s.best);
    CHECK_INT(cv_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
}

TEST(culvert_save_rejects)
{
    uint8_t buf[CV_SAVE_SIZE + 1];
    uint8_t bad[CV_SAVE_SIZE];
    cv_save_t s, t;
    uint32_t i, len;
    int v;

    sample_save(&s);
    cv_save_encode(&s, buf);
    buf[CV_SAVE_SIZE] = 0;
    for (i = 0; i < CV_SAVE_SIZE; i++) {
        for (v = 0; v < 256; v++) {
            if (v == buf[i]) {
                continue;
            }
            __builtin_memcpy(bad, buf, CV_SAVE_SIZE);
            bad[i] = (uint8_t)v;
            CHECK_INT(cv_save_decode(&t, bad, CV_SAVE_SIZE), -1);
            CHECK_INT(t.highest, 1);
            CHECK_INT(t.best[0], CV_NO_BEST);
        }
    }
    for (len = 0; len <= CV_SAVE_SIZE + 1; len++) {
        if (len != CV_SAVE_SIZE) {
            CHECK_INT(cv_save_decode(&t, buf, len), -1);
        }
    }
    CHECK_INT(cv_save_decode(&t, 0, CV_SAVE_SIZE), -1);
    /* An old version with a good checksum. */
    __builtin_memcpy(bad, buf, CV_SAVE_SIZE);
    bad[4] = 0;
    {
        uint32_t crc = cv_crc32(bad, CV_SAVE_SIZE - 4);

        bad[26] = (uint8_t)crc;
        bad[27] = (uint8_t)(crc >> 8);
        bad[28] = (uint8_t)(crc >> 16);
        bad[29] = (uint8_t)(crc >> 24);
    }
    CHECK_INT(cv_save_decode(&t, bad, CV_SAVE_SIZE), -1);
    /* Level 0 with a good checksum. */
    s.highest = 0;
    cv_save_encode(&s, bad);
    CHECK_INT(cv_save_decode(&t, bad, CV_SAVE_SIZE), -1);
}
