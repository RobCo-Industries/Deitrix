/*
 * Culvert's pure logic: pieces as 4-bit masks, the level generator, the
 * flood fill, the solved test, the seed rule, the timer and the save
 * record. No kernel calls and no globals, so the host tests link it.
 */
#ifndef CV_LOGIC_H
#define CV_LOGIC_H

#include <stdint.h>

/* The openings of a tile. A clockwise quarter turn moves each bit up. */
#define CV_N 1u
#define CV_E 2u
#define CV_S 4u
#define CV_W 8u

#define CV_MAX_W     10
#define CV_MAX_H     7
#define CV_MAX_TILES (CV_MAX_W * CV_MAX_H)

/* Grid sizes, smallest first. A level uses class (level - 1) / 3, at
 * most the last one, so the sizes grow every three levels. */
#define CV_CLASSES   9
#define CV_PER_CLASS 3

/* The highest level number the game counts to. */
#define CV_LEVEL_MAX 65535u

typedef enum {
    CV_PIECE_NONE,
    CV_PIECE_CAP,
    CV_PIECE_STRAIGHT,
    CV_PIECE_ELBOW,
    CV_PIECE_TEE,
    CV_PIECE_CROSS,
} cv_piece_t;

typedef struct {
    uint32_t level;
    uint32_t seed;
    uint8_t w, h;
    uint8_t sx, sy;                 /* the source tile */
    uint8_t mask[CV_MAX_TILES];     /* the tiles as the player sees them */
    uint8_t goal[CV_MAX_TILES];     /* the generator's solution */
} cv_level_t;

/* Pieces. */
uint8_t cv_rot_cw(uint8_t mask);
uint8_t cv_rot_ccw(uint8_t mask);
uint8_t cv_rot_n(uint8_t mask, int quarters);   /* any sign; negative is counter-clockwise */
cv_piece_t cv_piece(uint8_t mask);
int cv_bits(uint8_t mask);
/* The fewest clockwise quarter turns from `from` to `to`, or -1. */
int cv_turns_to(uint8_t from, uint8_t to);

/* Levels. */
uint32_t cv_seed(uint32_t level);
uint32_t cv_rng_next(uint32_t *state);
int cv_class(uint32_t level);
void cv_class_size(int cls, uint8_t *w, uint8_t *h);
void cv_generate(cv_level_t *lv, uint32_t level);

/* Water. `conn` gets 1 for every tile the source reaches. Returns the count. */
int cv_flood(const cv_level_t *lv, uint8_t conn[CV_MAX_TILES]);
/* Every tile reached and no opening into a wall or a closed side. */
int cv_solved(const cv_level_t *lv);
/* Turns tile (x, y) by `quarters`. */
void cv_turn(cv_level_t *lv, int x, int y, int quarters);

/* Time: the milliseconds from `start` to `now` on the 31-bit tick. */
uint32_t cv_elapsed(uint32_t start, uint32_t now);
/* "m:ss" into buf, at most 9 bytes with the NUL. Minutes stop at 999. */
void cv_fmt_time(char *buf, uint32_t ms);
/* A decimal number into buf, at most 11 bytes with the NUL. Returns the length. */
int cv_fmt_u32(char *buf, uint32_t v);

/* The save record. */
#define CV_SAVE_NAME    "CULVERT"
#define CV_SAVE_VERSION 1
#define CV_SAVE_SIZE    30
#define CV_NO_BEST      0xFFFFu

typedef struct {
    uint16_t highest;               /* the highest level reached, 1 or more */
    uint16_t best[CV_CLASSES];      /* the fewest moves per size class, or CV_NO_BEST */
} cv_save_t;

void cv_save_fresh(cv_save_t *s);
uint32_t cv_crc32(const uint8_t *p, uint32_t n);
void cv_save_encode(const cv_save_t *s, uint8_t out[CV_SAVE_SIZE]);
/* 0 and *s filled when the record is good, else -1 and *s fresh. */
int cv_save_decode(cv_save_t *s, const uint8_t *in, uint32_t len);

#endif
