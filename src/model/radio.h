/*
 * RADIO.INI and its files: the stations of the RADIO tab, and the .TUN
 * parser that turns a tune into the notes of one play_seq call.
 */
#ifndef DX_MODEL_RADIO_H
#define DX_MODEL_RADIO_H

#include <stdint.h>

#include "model/load.h"
#include "platform/platform.h"

#define DX_RADIO_LIMIT      2048
#define DX_STATIONS_MAX     6
#define DX_STATION_NAME_MAX 20
#define DX_TUN_LIMIT        4096
#define DX_TUN_LINE_MAX     127
#define DX_TUN_NOTES_MAX    256
#define DX_TUN_MS_MAX       120000u
#define DX_PCM_MIN          8000u
#define DX_PCM_MAX          2097152u

enum { DX_KIND_TUNE, DX_KIND_PCM };

typedef struct dx_station {
    int number;                         /* 1 to 6, from [station.N] */
    char name[DX_STATION_NAME_MAX + 1];
    uint8_t kind;                       /* DX_KIND_* */
    uint8_t loop;                       /* pcm: 1 for loop = yes */
    char file[DX_NAME83_SIZE];          /* in /PIPBOY/DATA/RADIO/ */
    uint8_t playable;                   /* its file loaded and follows the rules */
    dx_load_t file_load;                /* why it does not play */
    uint32_t pcm_size;                  /* pcm: bytes */
    dx_note_t notes[DX_TUN_NOTES_MAX];  /* tune: the play_seq notes */
    int note_count;
    uint32_t tune_ms;
} dx_station_t;

typedef struct dx_radio {
    dx_load_t load;
    dx_station_t stations[DX_STATIONS_MAX]; /* ascending number */
    int count;
} dx_radio_t;

/* Fills r from RADIO.INI. Every station starts not playable. */
void dx_radio_load(dx_radio_t *r, const uint8_t *data, int32_t size);

/* Station i's tune file, or the dx_plat_read_file error in size. */
void dx_radio_set_tune(dx_radio_t *r, int i, const uint8_t *data, int32_t size);
/* Station i's PCM file size, or a negative DX_FILE_* error. */
void dx_radio_set_pcm(dx_radio_t *r, int i, int32_t size);

/* The index of the station with this number, or -1. */
int dx_radio_find(const dx_radio_t *r, int number);

/*
 * Parses a .TUN file as tools/tun2notes.py does. Returns 1 and fills
 * out[0..*count), *total_ms. Else 0 and *err_line holds the line of the
 * first error, as the tool reports it.
 */
int dx_tun_parse(const uint8_t *data, int32_t size, dx_note_t *out, int *count,
                 uint32_t *total_ms, unsigned *err_line);

#endif
