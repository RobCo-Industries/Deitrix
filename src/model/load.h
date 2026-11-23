/*
 * What happened when a content file loaded, and the INI loading every
 * INI file shares: the size, the parse and the version key.
 */
#ifndef DX_MODEL_LOAD_H
#define DX_MODEL_LOAD_H

#include <stdint.h>

#include "model/util.h"

enum {
    DX_LOAD_NONE,      /* not loaded yet */
    DX_LOAD_OK,
    DX_LOAD_MISSING,
    DX_LOAD_TOO_LARGE,
    DX_LOAD_READ_ERROR,
    DX_LOAD_PARSE,     /* the parser stopped at `line` with `code` */
    DX_LOAD_VERSION,   /* no "version = 1" */
    DX_LOAD_BROKEN,    /* a tune that breaks a rule, at `line` */
};

typedef struct dx_load {
    int status;    /* DX_LOAD_* */
    unsigned line; /* the first error's line, 0 for none */
    int code;      /* the parser's code for DX_LOAD_PARSE */
} dx_load_t;

/* `size` from dx_plat_read_file: the size or a negative DX_FILE_*. 1
 * when it is a size, else 0 with the status set. */
int dx_load_size(dx_load_t *ld, int32_t size);

/*
 * Parses an INI file of `size` bytes, or the dx_plat_read_file error in
 * size. fn gets every key in file order except those of the section "".
 * Returns 1 when the file parsed and its version is 1. Else 0 and the
 * caller drops what fn kept.
 */
typedef int (*dx_load_key_t)(const char *section, const char *key, const char *value,
                             void *ctx);
int dx_load_ini(dx_load_t *ld, const uint8_t *data, int32_t size, dx_load_key_t fn, void *ctx);

/* Appends "ok", "missing" or "ignored: <reason>" to sb. */
void dx_load_reason(const dx_load_t *ld, dx_sb_t *sb);

#endif
