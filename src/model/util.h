/*
 * Small string and number helpers for the model: no C library, no
 * allocation. The rules follow docs/CONTENT.md and tools/check_content.py.
 */
#ifndef DX_MODEL_UTIL_H
#define DX_MODEL_UTIL_H

#include <stddef.h>
#include <stdint.h>

/* A string builder over a caller buffer. It never writes past cap and
 * always keeps a NUL; text that does not fit is cut. */
typedef struct dx_sb {
    char *buf;
    int cap;
    int len;
} dx_sb_t;

void dx_sb_init(dx_sb_t *sb, char *buf, int cap);
void dx_sb_str(dx_sb_t *sb, const char *s);
void dx_sb_int(dx_sb_t *sb, int32_t v);
void dx_sb_uint(dx_sb_t *sb, uint32_t v);
void dx_sb_char(dx_sb_t *sb, char c);

int dx_str_len(const char *s);
int dx_str_eq(const char *a, const char *b);
/* 1 when s ends with suffix. */
int dx_str_ends(const char *s, const char *suffix);
/* Byte order, as strcmp. */
int dx_str_cmp(const char *a, const char *b);
/* Copies at most cap - 1 bytes of the n bytes at src and a NUL: the cut. */
void dx_str_cut(char *dst, int cap, const char *src, int n);

void dx_mem_set(void *dst, uint8_t v, uint32_t n);
void dx_mem_copy(void *dst, const void *src, uint32_t n);

/* The n bytes at s without spaces at either end: *start and the new n. */
int dx_strip(const char *s, int n, int *start);

/*
 * An integer: 1 to 6 decimal digits after spaces at either end are
 * dropped. No sign, no 0x. Returns 1 and *out, else 0.
 */
int dx_parse_num(const char *s, int n, uint32_t *out);

/* N for a key "<prefix><N>", N 1 to max without a leading zero, else 0. */
int dx_numbered_key(const char *key, char prefix, int max);

/* 1 to 12 of a-z, 0-9, _: an item id or a category key. */
int dx_is_id(const char *s);
/* An upper-case 8.3 name: 1 to 8 of A-Z 0-9 _, an optional dot and 1 to 3 more. */
int dx_is_name83(const char *s);

/* Splits s at commas, at most `max` fields, the last one takes the rest.
 * Writes each field's start and length. Returns the field count. */
int dx_split(const char *s, int max, int *starts, int *lens);

#endif
