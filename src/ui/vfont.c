#include "ui/vfont.h"

/* Drawn for a character the font lacks. */
static const char box[] = "0040460600|0046|4006";

/* Index: 0-9, then A-Z. */
static const char *const alnum[36] = {
    "0040460600|0640",           /* 0, slashed */
    "1120|2026|1636",            /* 1 */
    "004043030646",              /* 2 */
    "00404606|1343",             /* 3 */
    "000343|4046",               /* 4 */
    "400003434606",              /* 5 */
    "400006464303",              /* 6 */
    "004016",                    /* 7 */
    "0040460600|0343",           /* 8 */
    "430300404606",              /* 9 */
    "0602204246|0444",           /* A */
    "00304142334445360600|0333", /* B */
    "40000646",                  /* C */
    "00304145360600",            /* D */
    "40000646|0333",             /* E */
    "400006|0333",               /* F */
    "400006464323",              /* G */
    "0006|4046|0343",            /* H */
    "0040|2026|0646",            /* I */
    "4045361605",                /* J */
    "0006|4004|2246",            /* K */
    "000646",                    /* L */
    "0600234046",                /* M */
    "06004640",                  /* N */
    "0040460600",                /* O */
    "0600404303",                /* P */
    "0040460600|2446",           /* Q */
    "0600404303|1346",           /* R */
    "413010010213334445361605",  /* S */
    "0040|2026",                 /* T */
    "00064640",                  /* U */
    "002640",                    /* V */
    "0016233640",                /* W */
    "0046|4006",                 /* X */
    "002340|2326",               /* Y */
    "00400646",                  /* Z */
};

const char *dx_vfont_glyph(char c)
{
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    if (c >= '0' && c <= '9') {
        return alnum[c - '0'];
    }
    if (c >= 'A' && c <= 'Z') {
        return alnum[10 + c - 'A'];
    }
    switch (c) {
    case ' ':
        return "";
    case ':':
        return "*0112|*0415";
    case '-':
        return "0343";
    case '.':
        return "*0516";
    case '/':
        return "0640";
    case '%':
        return "0640|0010110100|3545463635";
    default:
        return NULL;
    }
}

int dx_vfont_advance(char c)
{
    return (c == ':' || c == '.') ? 3 : 6;
}

int dx_vfont_width(const char *s, int unit)
{
    int w = 0;

    while (*s != '\0') {
        w += dx_vfont_advance(*s++) * unit;
    }
    return w;
}

int dx_vfont_weight(int unit)
{
    return unit >= 3 ? 2 : 1;
}

/* A weight 2 stroke is the line again 1 px right and 1 px down. */
static void stroke(const dx_ctx_t *ctx, int x0, int y0, int x1, int y1, int weight,
                   uint32_t color)
{
    ctx->gfx->line(x0, y0, x1, y1, color);
    if (weight > 1) {
        ctx->gfx->line(x0 + 1, y0, x1 + 1, y1, color);
        ctx->gfx->line(x0, y0 + 1, x1, y1 + 1, color);
    }
}

static void draw_glyph(const dx_ctx_t *ctx, int x, int y, const char *g, int unit, uint32_t color)
{
    int weight = dx_vfont_weight(unit);
    int have = 0;
    int px = 0, py = 0;

    while (*g != '\0') {
        int gx, gy;

        if (*g == '|') {
            have = 0;
            g++;
            continue;
        }
        if (*g == '*') {
            /* A filled square between two corners, as wide as strokes
             * around the same corners would reach. */
            int x0 = x + (g[1] - '0') * unit, y0 = y + (g[2] - '0') * unit;
            int x1 = x + (g[3] - '0') * unit, y1 = y + (g[4] - '0') * unit;

            ctx->gfx->rect(x0, y0, x1 - x0 + weight, y1 - y0 + weight, color, 1);
            have = 0;
            g += 5;
            continue;
        }
        gx = x + (g[0] - '0') * unit;
        gy = y + (g[1] - '0') * unit;
        if (have) {
            stroke(ctx, px, py, gx, gy, weight, color);
        }
        px = gx;
        py = gy;
        have = 1;
        g += 2;
    }
}

int dx_vfont_draw(const dx_ctx_t *ctx, int x, int y, const char *s, int unit, uint32_t color)
{
    int x0 = x;

    while (*s != '\0') {
        const char *g = dx_vfont_glyph(*s);

        draw_glyph(ctx, x, y, g != NULL ? g : box, unit, color);
        x += dx_vfont_advance(*s) * unit;
        s++;
    }
    return x - x0;
}
