/*
 * The platform layer: the only interface to Pip-OS. platform.c calls the
 * pip_* system calls. Everything else uses these functions and the
 * drawing table, so the host tests can link a fake instead
 * (tests/host/fake_platform.c).
 */
#ifndef DX_PLATFORM_H
#define DX_PLATFORM_H

#include <stdint.h>

/* Event types, button codes and press values, as in Pip-OS's ABI. */
enum { DX_EV_NONE = 0, DX_EV_BUTTON = 1, DX_EV_DIAL = 2 };
/* DX_BTN_BACK is new in ABI 1.2: a 1.0 or 1.1 Pip-OS never sends it. */
enum { DX_BTN_UP, DX_BTN_DOWN, DX_BTN_LEFT, DX_BTN_RIGHT, DX_BTN_SELECT, DX_BTN_DIAL, DX_BTN_BACK };
enum { DX_PRESS_RELEASE = 0, DX_PRESS_DOWN = 1, DX_PRESS_REPEAT = 2 };

typedef struct dx_event {
    uint32_t type;  /* DX_EV_* */
    uint32_t code;  /* DX_BTN_*, or 0 for the dial */
    int32_t value;  /* DX_PRESS_*, or signed dial detents */
} dx_event_t;

#define DX_SCREEN_W 320
#define DX_SCREEN_H 240
#define DX_FONT_W   8
#define DX_FONT_H   16
#define DX_SCALE_MAX 8
#define DX_POLY_MAX 256 /* points in one polyline */
#define DX_WAIT_FOREVER 0xFFFFFFFFu
#define DX_COLOR_NONE   0x01000000u /* a transparent background */

/*
 * The drawing calls. Colours are 0x00RRGGBB. A text background of
 * DX_COLOR_NONE is transparent. Every call clips to the clip rectangle
 * and to the screen.
 */
typedef struct dx_gfx {
    void (*clear)(uint32_t color);
    void (*line)(int x0, int y0, int x1, int y1, uint32_t color);
    void (*rect)(int x, int y, int w, int h, uint32_t color, int fill);
    /* Scale 1 to DX_SCALE_MAX. */
    void (*text)(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale);
    /* xy holds 2 to DX_POLY_MAX points, x then y. */
    void (*polyline)(const int16_t *xy, int points, int closed, uint32_t color);
    /* Degrees clockwise from +x, start 0 to 359, end 0 to 360. */
    void (*arc)(int cx, int cy, int r, int start_deg, int end_deg, uint32_t color, int fill);
    /* Rows packed most significant bit first. Set bits draw fg, clear bits bg. */
    void (*blit1)(int x, int y, int w, int h, const uint8_t *bits, int stride,
                  uint32_t fg, uint32_t bg);
    /* w or h 0: no clip. */
    void (*clip)(int x, int y, int w, int h);
    void (*present)(void);
} dx_gfx_t;

/* The drawing calls of this platform. */
const dx_gfx_t *dx_plat_gfx(void);

/* The foreground colour of the card's screen scheme, 0x00RRGGBB. */
uint32_t dx_plat_fg(void);

/* 1 and the Unix seconds, or 0 when Pip-OS has no clock. */
int dx_plat_time(uint32_t *unix_seconds);

/* Milliseconds since boot. Wraps. */
uint32_t dx_plat_ticks(void);

/* 1 with an event, 0 when timeout_ms passed first. */
int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms);

/* Writes text to the serial log as it is. */
void dx_plat_log(const char *text);

/* Starts a square-wave tone. It plays in the background. */
void dx_plat_tone(uint32_t freq_hz, uint32_t duration_ms);

/* ---- Files: read only, on the card --------------------------------------- */

/* What dx_plat_read_file returns besides a size. */
#define DX_FILE_MISSING   (-1)
#define DX_FILE_TOO_LARGE (-2)
#define DX_FILE_ERROR     (-3)

/*
 * Reads the whole file at path into buf, at most `limit` bytes. Returns
 * its size, DX_FILE_MISSING, DX_FILE_TOO_LARGE when it holds more than
 * `limit` bytes, or DX_FILE_ERROR.
 */
int32_t dx_plat_read_file(const char *path, uint8_t *buf, uint32_t limit);

/* An open file for streaming: a handle 0 to 7, or a negative error. */
#define DX_EBUSY (-16) /* -PIP_EBUSY */
int32_t dx_plat_file_open(const char *path);
/* Bytes read, 0 at the end, or a negative error. */
int32_t dx_plat_file_read(int32_t handle, uint8_t *buf, uint32_t size);
void dx_plat_file_close(int32_t handle);

#define DX_NAME83_SIZE 13 /* an 8.3 name and its NUL */

typedef struct dx_dirent {
    char name[DX_NAME83_SIZE];
    uint8_t is_dir;
    uint32_t size;
} dx_dirent_t;

/* Lists entries `start` and on of the folder at path into out, at most
 * max. Returns the entries written, 0 past the end, DX_FILE_MISSING or
 * DX_FILE_ERROR. */
int32_t dx_plat_dir_list(const char *path, dx_dirent_t *out, uint32_t max, uint32_t start);

/* ---- Saves: /PIPBOY/SAVE/<name>.SAV --------------------------------------- */

/* Bytes read, at most size, DX_FILE_MISSING or DX_FILE_ERROR. */
int32_t dx_plat_save_read(const char *name, uint8_t *buf, uint32_t size);
/* 0, or DX_FILE_ERROR. */
int32_t dx_plat_save_write(const char *name, const uint8_t *buf, uint32_t size);
/* 0, DX_FILE_MISSING or DX_FILE_ERROR. */
int32_t dx_plat_save_delete(const char *name);

/* ---- Audio: one voice ---------------------------------------------------- */

/* The layout of Pip-OS's pip_note_t: freq_hz 0 is a rest. */
typedef struct dx_note {
    uint16_t freq_hz;
    uint16_t duration_ms;
} dx_note_t;

#define DX_SAMPLE_MAX 65536 /* bytes per sample call */

/* Each returns 0 or a negative error. sample_queue returns DX_EBUSY when
 * a sample already waits. */
int32_t dx_plat_play_seq(const dx_note_t *notes, uint32_t count);
int32_t dx_plat_sample(const uint8_t *data, uint32_t length);
int32_t dx_plat_sample_queue(const uint8_t *data, uint32_t length);
void dx_plat_audio_stop(void);
/* 1 while a sound plays or a queued sample waits, else 0. */
int dx_plat_audio_status(void);
/* Sets the level, 0 to 100. Returns the previous one, or -1. */
int32_t dx_plat_volume(uint32_t level);

/* ---- System information, for the STAT tab ---------------------------------- */

typedef struct dx_sysinfo {
    uint32_t abi_version;   /* (major << 16) | minor */
    uint32_t kernel_version; /* (major << 16) | (minor << 8) | patch */
    uint32_t board_rev;
    uint32_t soc;           /* 0x2835, 0x2836 or 0x2837 */
    uint32_t mem_total;     /* RAM in bytes */
    uint32_t heap_size;     /* the ROM's heap region in bytes */
    uint32_t stack_size;    /* the ROM's stack in bytes */
} dx_sysinfo_t;

/* Fills *out. 0, or -1 with *out zeroed. */
int dx_plat_sysinfo(dx_sysinfo_t *out);

/* ---- Library code: libpip on the target, RobCo-Core on the host ----------- */

/* As pip_ini_parse in <pipos/ini.h>: one call per key in file order.
 * Returns 0 at the end, 1 when the handler stopped, or a negative code. */
typedef int (*dx_ini_handler_t)(const char *section, const char *key, const char *value,
                                unsigned line, void *ctx);
int dx_ini_parse(const char *text, uint32_t len, dx_ini_handler_t fn, void *ctx,
                 unsigned *err_line);
/* Short text for a negative dx_ini_parse code. */
const char *dx_ini_strerror(int code);

/* The common CRC-32, as zlib's crc32(). */
uint32_t dx_crc32(const void *data, uint32_t n);

#endif
