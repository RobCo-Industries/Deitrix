#include "model/load.h"

#include "platform/platform.h"

typedef struct ini_ctx {
    dx_load_key_t fn;
    void *ctx;
    int version_ok;
} ini_ctx_t;

int dx_load_size(dx_load_t *ld, int32_t size)
{
    ld->line = 0;
    ld->code = 0;
    if (size >= 0) {
        ld->status = DX_LOAD_OK;
        return 1;
    }
    ld->status = size == DX_FILE_MISSING     ? DX_LOAD_MISSING
                 : size == DX_FILE_TOO_LARGE ? DX_LOAD_TOO_LARGE
                                             : DX_LOAD_READ_ERROR;
    return 0;
}

static int on_key(const char *section, const char *key, const char *value, unsigned line,
                  void *p)
{
    ini_ctx_t *c = p;

    (void)line;
    if (section[0] == '\0') {
        /* Only version counts here, and the last one wins. */
        if (dx_str_eq(key, "version")) {
            c->version_ok = dx_str_eq(value, "1");
        }
        return 0;
    }
    return c->fn(section, key, value, c->ctx);
}

int dx_load_ini(dx_load_t *ld, const uint8_t *data, int32_t size, dx_load_key_t fn, void *ctx)
{
    ini_ctx_t c;
    unsigned line = 0;
    int rc;

    if (!dx_load_size(ld, size)) {
        return 0;
    }
    c.fn = fn;
    c.ctx = ctx;
    c.version_ok = 0;
    rc = dx_ini_parse((const char *)data, (uint32_t)size, on_key, &c, &line);
    if (rc < 0) {
        ld->status = DX_LOAD_PARSE;
        ld->line = line;
        ld->code = rc;
        return 0;
    }
    if (!c.version_ok) {
        ld->status = DX_LOAD_VERSION;
        return 0;
    }
    return 1;
}

void dx_load_reason(const dx_load_t *ld, dx_sb_t *sb)
{
    switch (ld->status) {
    case DX_LOAD_OK:
        dx_sb_str(sb, "ok");
        return;
    case DX_LOAD_MISSING:
        dx_sb_str(sb, "missing");
        return;
    case DX_LOAD_NONE:
        dx_sb_str(sb, "not loaded");
        return;
    default:
        break;
    }
    dx_sb_str(sb, "ignored: ");
    switch (ld->status) {
    case DX_LOAD_TOO_LARGE:
        dx_sb_str(sb, "too large");
        break;
    case DX_LOAD_TOO_SMALL:
        dx_sb_str(sb, "too small");
        break;
    case DX_LOAD_READ_ERROR:
        dx_sb_str(sb, "read error");
        break;
    case DX_LOAD_VERSION:
        dx_sb_str(sb, "bad version");
        break;
    case DX_LOAD_PARSE:
        dx_sb_str(sb, "line ");
        dx_sb_uint(sb, ld->line);
        dx_sb_str(sb, ": ");
        dx_sb_str(sb, dx_ini_strerror(ld->code));
        break;
    default:
        dx_sb_str(sb, "line ");
        dx_sb_uint(sb, ld->line);
        break;
    }
}
