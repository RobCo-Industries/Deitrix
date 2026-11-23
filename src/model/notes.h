/*
 * NOTES: the list of the DATA tab, built from the folder listing, and
 * the body of one note, read on demand.
 */
#ifndef DX_MODEL_NOTES_H
#define DX_MODEL_NOTES_H

#include <stdint.h>

#include "model/load.h"
#include "platform/platform.h"

#define DX_NOTES_MAX      32
#define DX_NOTE_FILE_MAX  4096
#define DX_NOTE_TITLE_MAX 30

typedef struct dx_notefile {
    char file[DX_NAME83_SIZE];
    char title[DX_NOTE_TITLE_MAX + 1]; /* empty until read: then not shown */
    uint32_t size;
} dx_notefile_t;

typedef struct dx_notes {
    dx_load_t load;                     /* the folder: OK, MISSING or READ_ERROR */
    dx_notefile_t notes[DX_NOTES_MAX];  /* in name order */
    int count;
} dx_notes_t;

/* Starts an empty list. */
void dx_notes_begin(dx_notes_t *n);

/* Offers one folder entry. A file with an upper-case 8.3 name ending in
 * .TXT is kept when it is among the first 32 in name order. */
void dx_notes_offer(dx_notes_t *n, const dx_dirent_t *e);

/* Sets the title of note i from the file's bytes, or from the
 * dx_plat_read_file error in size. */
void dx_notes_title(dx_notes_t *n, int i, const uint8_t *data, int32_t size);

/* Drops the notes that are not shown: too large, unread, or with an
 * empty title. */
void dx_notes_finish(dx_notes_t *n);

/*
 * The body of a note from its file's bytes: everything after the first
 * line, CR LF turned into LF, any other control byte but tab turned into
 * '?'. Writes at most cap - 1 bytes and a NUL into out. Returns the
 * length.
 */
int dx_notes_body(const uint8_t *data, uint32_t size, char *out, int cap);

#endif
