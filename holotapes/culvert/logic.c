/*
 * Culvert's pure logic. See logic.h and docs/ARCHITECTURE.md, "Culvert".
 */
#include "logic.h"

static const uint8_t class_w[CV_CLASSES] = { 5, 6, 6, 7, 8, 8, 9, 10, 10 };
static const uint8_t class_h[CV_CLASSES] = { 4, 4, 5, 5, 5, 6, 6, 6, 7 };

/* Neighbour offsets and the opposite side, in the order N, E, S, W. */
static const int8_t dir_dx[4] = { 0, 1, 0, -1 };
static const int8_t dir_dy[4] = { -1, 0, 1, 0 };

static uint8_t opposite(uint8_t bit)
{
    return cv_rot_n(bit, 2);
}

uint8_t cv_rot_cw(uint8_t mask)
{
    mask &= 15u;
    return (uint8_t)(((mask << 1) | (mask >> 3)) & 15u);
}

uint8_t cv_rot_ccw(uint8_t mask)
{
    mask &= 15u;
    return (uint8_t)(((mask >> 1) | (mask << 3)) & 15u);
}

uint8_t cv_rot_n(uint8_t mask, int quarters)
{
    int n = quarters % 4;

    if (n < 0) {
        n += 4;
    }
    while (n-- > 0) {
        mask = cv_rot_cw(mask);
    }
    return (uint8_t)(mask & 15u);
}

int cv_bits(uint8_t mask)
{
    int n = 0;
    int i;

    for (i = 0; i < 4; i++) {
        n += (mask >> i) & 1;
    }
    return n;
}

cv_piece_t cv_piece(uint8_t mask)
{
    switch (cv_bits(mask)) {
    case 1:
        return CV_PIECE_CAP;
    case 2:
        return (mask & 15u) == (CV_N | CV_S) || (mask & 15u) == (CV_E | CV_W) ? CV_PIECE_STRAIGHT
                                                                            : CV_PIECE_ELBOW;
    case 3:
        return CV_PIECE_TEE;
    case 4:
        return CV_PIECE_CROSS;
    default:
        return CV_PIECE_NONE;
    }
}

int cv_turns_to(uint8_t from, uint8_t to)
{
    int n;

    for (n = 0; n < 4; n++) {
        if (cv_rot_n(from, n) == (to & 15u)) {
            return n;
        }
    }
    return -1;
}

/* The seed of a level: MurmurHash3's 32-bit finaliser over the level
 * number mixed with a constant. Never 0, so the generator never sticks. */
uint32_t cv_seed(uint32_t level)
{
    uint32_t x = level ^ 0x43554C56u;

    x ^= x >> 16;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return x != 0 ? x : 1u;
}

/* xorshift32. */
uint32_t cv_rng_next(uint32_t *state)
{
    uint32_t x = *state != 0 ? *state : 1u;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int cv_class(uint32_t level)
{
    uint32_t c = level > 0 ? (level - 1) / CV_PER_CLASS : 0;

    return c >= CV_CLASSES ? CV_CLASSES - 1 : (int)c;
}

void cv_class_size(int cls, uint8_t *w, uint8_t *h)
{
    if (cls < 0) {
        cls = 0;
    } else if (cls >= CV_CLASSES) {
        cls = CV_CLASSES - 1;
    }
    *w = class_w[cls];
    *h = class_h[cls];
}

/* A random spanning tree grown from the source: each step takes a
 * random edge from the tree to a tile outside it. */
static void grow_tree(cv_level_t *lv, uint32_t *rng)
{
    uint8_t in[CV_MAX_TILES];
    uint8_t edge_tile[CV_MAX_TILES * 4];
    uint8_t edge_dir[CV_MAX_TILES * 4];
    int n = lv->w * lv->h;
    int edges = 0;
    int t = lv->sy * lv->w + lv->sx;
    int i;

    for (i = 0; i < n; i++) {
        in[i] = 0;
        lv->goal[i] = 0;
    }
    for (;;) {
        int x = t % lv->w;
        int y = t / lv->w;
        int d;

        in[t] = 1;
        for (d = 0; d < 4; d++) {
            int nx = x + dir_dx[d];
            int ny = y + dir_dy[d];

            if (nx >= 0 && ny >= 0 && nx < lv->w && ny < lv->h && !in[ny * lv->w + nx]) {
                edge_tile[edges] = (uint8_t)t;
                edge_dir[edges] = (uint8_t)d;
                edges++;
            }
        }
        t = -1;
        while (edges > 0 && t < 0) {
            int k = (int)(cv_rng_next(rng) % (uint32_t)edges);
            int from = edge_tile[k];
            int d2 = edge_dir[k];
            int to = (from / lv->w + dir_dy[d2]) * lv->w + from % lv->w + dir_dx[d2];

            edges--;
            edge_tile[k] = edge_tile[edges];
            edge_dir[k] = edge_dir[edges];
            if (!in[to]) {
                lv->goal[from] |= (uint8_t)(1u << d2);
                lv->goal[to] |= opposite((uint8_t)(1u << d2));
                t = to;
            }
        }
        if (t < 0) {
            return;
        }
    }
}

void cv_generate(cv_level_t *lv, uint32_t level)
{
    uint32_t rng;
    int n;
    int i;

    if (level == 0) {
        level = 1;
    }
    lv->level = level;
    lv->seed = cv_seed(level);
    cv_class_size(cv_class(level), &lv->w, &lv->h);
    lv->sx = (uint8_t)(lv->w / 2);
    lv->sy = (uint8_t)(lv->h / 2);
    rng = lv->seed;
    grow_tree(lv, &rng);
    n = lv->w * lv->h;
    for (i = 0; i < CV_MAX_TILES; i++) {
        lv->mask[i] = 0;
        if (i >= n) {
            lv->goal[i] = 0;
        }
    }
    for (i = 0; i < n; i++) {
        lv->mask[i] = cv_rot_n(lv->goal[i], (int)(cv_rng_next(&rng) % 4u));
    }
    /* A scramble that left the level solved gets one more turn: any
     * piece but a cross then leaks into the side it left. */
    if (cv_solved(lv)) {
        for (i = 0; i < n; i++) {
            if (cv_piece(lv->mask[i]) != CV_PIECE_CROSS) {
                lv->mask[i] = cv_rot_cw(lv->mask[i]);
                break;
            }
        }
    }
}

/* The opening `bit` of tile (x, y) meets an opening of the neighbour. */
static int open_to(const cv_level_t *lv, int x, int y, int d)
{
    int nx = x + dir_dx[d];
    int ny = y + dir_dy[d];
    uint8_t bit = (uint8_t)(1u << d);

    if (!(lv->mask[y * lv->w + x] & bit)) {
        return 0;
    }
    if (nx < 0 || ny < 0 || nx >= lv->w || ny >= lv->h) {
        return 0;
    }
    return (lv->mask[ny * lv->w + nx] & opposite(bit)) != 0;
}

int cv_flood(const cv_level_t *lv, uint8_t conn[CV_MAX_TILES])
{
    uint8_t queue[CV_MAX_TILES];
    int n = lv->w * lv->h;
    int head = 0;
    int tail = 0;
    int i;

    for (i = 0; i < CV_MAX_TILES; i++) {
        conn[i] = 0;
    }
    if (n <= 0 || lv->sx >= lv->w || lv->sy >= lv->h) {
        return 0;
    }
    queue[tail++] = (uint8_t)(lv->sy * lv->w + lv->sx);
    conn[queue[0]] = 1;
    while (head < tail) {
        int t = queue[head++];
        int x = t % lv->w;
        int y = t / lv->w;
        int d;

        for (d = 0; d < 4; d++) {
            if (open_to(lv, x, y, d)) {
                int u = (y + dir_dy[d]) * lv->w + x + dir_dx[d];

                if (!conn[u]) {
                    conn[u] = 1;
                    queue[tail++] = (uint8_t)u;
                }
            }
        }
    }
    return tail;
}

int cv_solved(const cv_level_t *lv)
{
    uint8_t conn[CV_MAX_TILES];
    int n = lv->w * lv->h;
    int x, y, d;

    if (cv_flood(lv, conn) != n) {
        return 0;
    }
    for (y = 0; y < lv->h; y++) {
        for (x = 0; x < lv->w; x++) {
            for (d = 0; d < 4; d++) {
                if ((lv->mask[y * lv->w + x] & (1u << d)) && !open_to(lv, x, y, d)) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

void cv_turn(cv_level_t *lv, int x, int y, int quarters)
{
    if (x < 0 || y < 0 || x >= lv->w || y >= lv->h) {
        return;
    }
    lv->mask[y * lv->w + x] = cv_rot_n(lv->mask[y * lv->w + x], quarters);
}

uint32_t cv_elapsed(uint32_t start, uint32_t now)
{
    return (now - start) & 0x7FFFFFFFu;
}

int cv_fmt_u32(char *buf, uint32_t v)
{
    char tmp[10];
    int n = 0;
    int i;

    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0);
    for (i = 0; i < n; i++) {
        buf[i] = tmp[n - 1 - i];
    }
    buf[n] = '\0';
    return n;
}

void cv_fmt_time(char *buf, uint32_t ms)
{
    uint32_t s = ms / 1000u;
    uint32_t m = s / 60u;
    int n;

    if (m > 999u) {
        m = 999u;
        s = 59u;
    } else {
        s %= 60u;
    }
    n = cv_fmt_u32(buf, m);
    buf[n++] = ':';
    buf[n++] = (char)('0' + s / 10u);
    buf[n++] = (char)('0' + s % 10u);
    buf[n] = '\0';
}

void cv_save_fresh(cv_save_t *s)
{
    int i;

    s->highest = 1;
    for (i = 0; i < CV_CLASSES; i++) {
        s->best[i] = CV_NO_BEST;
    }
}

/* CRC-32/ISO-HDLC, bit by bit: the save is 30 bytes. */
uint32_t cv_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    int b;

    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint32_t get16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

/* Layout, little-endian: "CVLT", version, class count, highest level,
 * the best moves of each class, then the CRC-32 of the 26 bytes before. */
void cv_save_encode(const cv_save_t *s, uint8_t out[CV_SAVE_SIZE])
{
    uint32_t crc;
    int i;

    out[0] = 'C';
    out[1] = 'V';
    out[2] = 'L';
    out[3] = 'T';
    out[4] = CV_SAVE_VERSION;
    out[5] = CV_CLASSES;
    put16(out + 6, s->highest);
    for (i = 0; i < CV_CLASSES; i++) {
        put16(out + 8 + 2 * i, s->best[i]);
    }
    crc = cv_crc32(out, CV_SAVE_SIZE - 4);
    put16(out + CV_SAVE_SIZE - 4, crc & 0xFFFFu);
    put16(out + CV_SAVE_SIZE - 2, crc >> 16);
}

int cv_save_decode(cv_save_t *s, const uint8_t *in, uint32_t len)
{
    uint32_t crc;
    int i;

    cv_save_fresh(s);
    if (in == 0 || len != CV_SAVE_SIZE) {
        return -1;
    }
    if (in[0] != 'C' || in[1] != 'V' || in[2] != 'L' || in[3] != 'T' ||
        in[4] != CV_SAVE_VERSION || in[5] != CV_CLASSES) {
        return -1;
    }
    crc = get16(in + CV_SAVE_SIZE - 4) | (get16(in + CV_SAVE_SIZE - 2) << 16);
    if (crc != cv_crc32(in, CV_SAVE_SIZE - 4)) {
        return -1;
    }
    if (get16(in + 6) == 0) {
        return -1;
    }
    s->highest = (uint16_t)get16(in + 6);
    for (i = 0; i < CV_CLASSES; i++) {
        s->best[i] = (uint16_t)get16(in + 8 + 2 * i);
    }
    return 0;
}
