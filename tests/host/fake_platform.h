/*
 * A fake platform layer for the host tests. Its drawing table records
 * every call as one line of a draw list, plus the text and rectangle
 * calls as structs. It also records log lines and tones, and plays back
 * scripted events, clock readings and ticks.
 */
#ifndef DX_FAKE_PLATFORM_H
#define DX_FAKE_PLATFORM_H

#include <stdint.h>

#include "platform/platform.h"
#include "ui/ui.h"

#define FAKE_MAX_TEXTS  128
#define FAKE_MAX_RECTS  64
#define FAKE_LOG_SIZE   2048
#define FAKE_MAX_EVENTS 64
#define FAKE_DRAW_SIZE  16384
#define FAKE_MAX_TONES  32
#define FAKE_MAX_POINTS 512
#define FAKE_MAX_FILES  80
#define FAKE_PATH_MAX   48
#define FAKE_MAX_SAVES  4
#define FAKE_SAVE_MAX   2048
#define FAKE_MAX_AUDIO  256
#define FAKE_HANDLES    8

/* A file on the fake card. data NULL: every byte is `fill`. */
typedef struct fake_file {
    char path[FAKE_PATH_MAX];
    const uint8_t *data;
    uint32_t size;
    uint8_t fill;
} fake_file_t;

typedef struct fake_save {
    char name[16];
    uint8_t data[FAKE_SAVE_MAX];
    uint32_t size;
} fake_save_t;

enum { FAKE_AU_SEQ, FAKE_AU_SAMPLE, FAKE_AU_QUEUE, FAKE_AU_BUSY, FAKE_AU_STOP, FAKE_AU_VOLUME,
       FAKE_AU_TONE };

/* One audio call: op, the note count or byte length or level, and the
 * first note's frequency or the first byte. */
typedef struct fake_audio {
    int op;
    uint32_t n;
    uint32_t first;
    uint32_t at; /* fake.ticks at the call */
} fake_audio_t;

typedef struct fake_text {
    int32_t x, y;
    int scale;
    uint32_t fg, bg;
    char text[48];
} fake_text_t;

typedef struct fake_rect {
    int32_t x, y, w, h;
    int filled;
    uint32_t fg;
} fake_rect_t;

/* A point any call touched, for the inside-the-area checks. */
typedef struct fake_point {
    int x, y;
} fake_point_t;

typedef struct fake_platform {
    /* Inputs the test sets. */
    uint32_t fg;
    int clock_valid;
    uint32_t clock;
    uint32_t ticks;
    uint32_t tick_step; /* added to ticks at each wait */
    uint32_t draw_ms;   /* added to ticks at each present: a slow draw */
    uint32_t draw_s;    /* added to the clock at each present */
    dx_event_t events[FAKE_MAX_EVENTS];
    int event_count;
    /* The theme the draw list names colours by. */
    dx_theme_t theme;
    /* What the code under test did. */
    int event_next;
    uint32_t last_timeout;
    int waits;
    int presents;
    int clears;
    fake_text_t texts[FAKE_MAX_TEXTS];
    int text_count;
    fake_rect_t rects[FAKE_MAX_RECTS];
    int rect_count;
    /* Corners of rects, ends of lines, text boxes, blits. */
    fake_point_t points[FAKE_MAX_POINTS];
    int point_count;
    char draws[FAKE_DRAW_SIZE];
    char log[FAKE_LOG_SIZE];
    uint32_t tones[FAKE_MAX_TONES][2];
    int tone_count;
    /* The card: files, open handles, failures to inject. */
    fake_file_t files[FAKE_MAX_FILES];
    int file_count;
    int32_t handle_file[FAKE_HANDLES]; /* file index, -1 when closed */
    uint32_t handle_pos[FAKE_HANDLES];
    int handles_open;
    int read_fail;       /* dx_plat_read_file and file_read fail while set */
    int dir_page;        /* dir_list returns at most this many, 0: 64 */
    /* The save store. save_fail: the next N writes fail. */
    fake_save_t saves[FAKE_MAX_SAVES];
    int save_count;
    int save_fail;
    int save_writes;
    /* Audio: the calls, and the voice played against fake.ticks. */
    fake_audio_t audio[FAKE_MAX_AUDIO];
    int audio_count;
    uint32_t volume;
    int au_on;           /* a sound started and not stopped */
    uint32_t au_end;     /* tick when the playing sound ends */
    uint32_t au_wait;    /* ms of the waiting sample, 0 for none */
    int au_starts;       /* sample, or sample_queue with nothing playing */
    int au_nexts;        /* sample_queue behind a playing sample */
    uint32_t au_crc;     /* CRC-32 of every byte sample and sample_queue took */
    uint32_t au_bytes;
    int seq_count;       /* play_seq calls */
    uint32_t seq_ms;     /* the length of the last sequence */
    dx_sysinfo_t sysinfo;
} fake_platform_t;

extern fake_platform_t fake;

/* Clears every field, with fg 0x33e066, its theme and no clock. */
void fake_reset(void);

/* Queues one event for dx_plat_wait. */
void fake_push(uint32_t type, uint32_t code, int32_t value);

/* Forgets the draw calls so far, keeps the inputs and the log. */
void fake_clear_draws(void);

/* The recorded text call with this text, or NULL. */
const fake_text_t *fake_find_text(const char *text);

/* A context that draws into the recorder with fake.theme. */
dx_ctx_t fake_ctx(void);

/* 1 when every recorded point lies inside r. */
int fake_points_inside(dx_rect_t r);

/* Adds a file to the fake card. data is not copied. */
void fake_add_file(const char *path, const void *data, uint32_t size);
/* Adds a file of `size` bytes, all `fill`. */
void fake_add_fill(const char *path, uint32_t size, uint8_t fill);
/* Adds a NUL-terminated text as a file. */
void fake_add_text(const char *path, const char *text);
/* Adds every file under the disk folder `dir` to the card under
 * `prefix`, recursively. Returns the file count, -1 on an error. The
 * bytes stay allocated until the program ends. */
int fake_load_tree(const char *dir, const char *prefix);
/* Removes a file. */
void fake_remove_file(const char *path);
/* The save by name, or NULL. */
fake_save_t *fake_find_save(const char *name);
/* Stores a save as if written before. */
void fake_put_save(const char *name, const void *data, uint32_t size);
/* 1 while the fake voice plays or a sample waits, at fake.ticks. */
int fake_audio_busy(void);
/* How many audio calls of `op`. */
int fake_audio_ops(int op);

#endif
