/*
 * PROFILE.INI: the owner, gauges, attributes and about lines of the
 * STAT tab, as docs/CONTENT.md defines them.
 */
#ifndef DX_MODEL_PROFILE_H
#define DX_MODEL_PROFILE_H

#include <stdint.h>

#include "model/load.h"

#define DX_PROFILE_LIMIT     4096
#define DX_OWNER_NAME_MAX    24
#define DX_OWNER_LINE_MAX    38
#define DX_GAUGES_MAX        6
#define DX_GAUGE_NAME_MAX    16
#define DX_GAUGE_VALUE_MAX   9999
#define DX_ATTRS_MAX         12
#define DX_ATTR_NAME_MAX     16
#define DX_ATTR_VALUE_MAX    99
#define DX_ABOUT_MAX         8
#define DX_ABOUT_LINE_MAX    38

typedef struct dx_gauge {
    uint16_t value; /* 0 to max */
    uint16_t max;   /* 1 to 9999 */
    char name[DX_GAUGE_NAME_MAX + 1];
} dx_gauge_t;

typedef struct dx_attr {
    uint8_t value; /* 0 to 99 */
    char name[DX_ATTR_NAME_MAX + 1];
} dx_attr_t;

typedef struct dx_profile {
    dx_load_t load;
    char name[DX_OWNER_NAME_MAX + 1]; /* empty: no name row */
    char line[DX_OWNER_LINE_MAX + 1];
    /* Entries in ascending N, gaps closed. */
    dx_gauge_t gauges[DX_GAUGES_MAX];
    int gauge_count;
    dx_attr_t attrs[DX_ATTRS_MAX];
    int attr_count;
    char about[DX_ABOUT_MAX][DX_ABOUT_LINE_MAX + 1];
    int about_count;
} dx_profile_t;

/* Fills p from the file's bytes, or from the dx_plat_read_file error in
 * size. A file that is ignored leaves p empty with its reason in load. */
void dx_profile_load(dx_profile_t *p, const uint8_t *data, int32_t size);

#endif
