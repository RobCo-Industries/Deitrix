/*
 * MAP.INI: the local and world maps of the MAP tab, and the integer
 * transform that fits a map's box into a pixel rectangle.
 */
#ifndef DX_MODEL_MAP_H
#define DX_MODEL_MAP_H

#include <stdint.h>

#include "model/load.h"

#define DX_MAP_LIMIT      32768
#define DX_MAPS_MAX       2
#define DX_MAP_TITLE_MAX  24
#define DX_COORD_MAX      999
#define DX_LINES_MAX      64
#define DX_POINTS_MIN     2
#define DX_POINTS_MAX     16
#define DX_PLACES_MAX     24
#define DX_PLACE_NAME_MAX 24

enum { DX_MAP_LOCAL, DX_MAP_WORLD };
enum { DX_PLACE_HOME, DX_PLACE_WATER, DX_PLACE_SHELTER, DX_PLACE_MARKER, DX_PLACE_HAZARD };

typedef struct dx_point {
    int16_t x, y;
} dx_point_t;

typedef struct dx_polyline {
    int count; /* 2 to 16 */
    dx_point_t pts[DX_POINTS_MAX];
} dx_polyline_t;

typedef struct dx_place {
    dx_point_t at;
    uint8_t kind; /* DX_PLACE_* */
    char name[DX_PLACE_NAME_MAX + 1];
} dx_place_t;

typedef struct dx_map {
    int which; /* DX_MAP_LOCAL or DX_MAP_WORLD */
    char title[DX_MAP_TITLE_MAX + 1];
    int16_t box[4]; /* x0, y0, x1, y1: x0 < x1, y0 < y1 */
    dx_polyline_t lines[DX_LINES_MAX]; /* ascending N, gaps closed */
    int line_count;
    dx_place_t places[DX_PLACES_MAX];
    int place_count;
} dx_map_t;

typedef struct dx_maps {
    dx_load_t load;
    dx_map_t maps[DX_MAPS_MAX]; /* local first when present */
    int count;
} dx_maps_t;

void dx_map_load(dx_maps_t *m, const uint8_t *data, int32_t size);

/* The kind name of a DX_PLACE_*, or "". */
const char *dx_place_kind_name(int kind);

/*
 * Fits a box into the pixel rectangle x, y, w, h, keeping its aspect
 * ratio and centring it. Map x0 lands on the left pixel or further in,
 * x1 on the last pixel or before; the same for y. A rectangle with no
 * area or a box with no width puts every point at the rectangle's
 * top-left corner, with scale 0.
 */
typedef struct dx_map_fit {
    int ox, oy;   /* the pixel of box corner x0, y0 */
    int num, den; /* pixels per map unit: num / den */
    int x0, y0;
} dx_map_fit_t;

dx_map_fit_t dx_map_fit(const int16_t box[4], int x, int y, int w, int h);
/* A map point to a pixel, rounded to the nearest. */
void dx_map_to_px(const dx_map_fit_t *f, int mx, int my, int *px, int *py);

#endif
