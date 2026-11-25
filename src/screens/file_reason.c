#include "screens/screens.h"

#include "model/load.h"

const char *dx_file_reason(const struct dx_load *ld, const char *file, char *buf, int cap)
{
    dx_sb_t sb;

    dx_sb_init(&sb, buf, cap);
    dx_sb_str(&sb, file);
    dx_sb_char(&sb, ' ');
    switch (ld->status) {
    case DX_LOAD_MISSING:
        dx_sb_str(&sb, "missing");
        break;
    case DX_LOAD_TOO_LARGE:
        dx_sb_str(&sb, "too large");
        break;
    case DX_LOAD_TOO_SMALL:
        dx_sb_str(&sb, "too small");
        break;
    case DX_LOAD_VERSION:
        dx_sb_str(&sb, "bad version");
        break;
    case DX_LOAD_PARSE:
    case DX_LOAD_BROKEN:
        dx_sb_str(&sb, "line ");
        dx_sb_uint(&sb, ld->line);
        break;
    default:
        dx_sb_str(&sb, "read error");
        break;
    }
    return buf;
}
