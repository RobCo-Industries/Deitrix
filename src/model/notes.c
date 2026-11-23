#include "model/notes.h"

void dx_notes_begin(dx_notes_t *n)
{
    dx_mem_set(n, 0, sizeof(*n));
    n->load.status = DX_LOAD_OK;
}

void dx_notes_offer(dx_notes_t *n, const dx_dirent_t *e)
{
    int i;
    int k;

    if (e->is_dir || !dx_is_name83(e->name) || !dx_str_ends(e->name, ".TXT")) {
        return;
    }
    /* Insertion into the sorted list; the last falls off when full. */
    for (i = n->count; i > 0; i--) {
        int c = dx_str_cmp(n->notes[i - 1].file, e->name);

        if (c == 0) {
            return;
        }
        if (c < 0) {
            break;
        }
    }
    if (i >= DX_NOTES_MAX) {
        return;
    }
    if (n->count < DX_NOTES_MAX) {
        n->count++;
    }
    for (k = n->count - 1; k > i; k--) {
        n->notes[k] = n->notes[k - 1];
    }
    dx_mem_set(&n->notes[i], 0, sizeof(n->notes[0]));
    dx_str_cut(n->notes[i].file, DX_NAME83_SIZE, e->name, dx_str_len(e->name));
    n->notes[i].size = e->size;
}

void dx_notes_title(dx_notes_t *n, int i, const uint8_t *data, int32_t size)
{
    int len = 0;
    int k;
    int blank = 1;

    if (i < 0 || i >= n->count) {
        return;
    }
    n->notes[i].title[0] = '\0';
    if (size < 0 || size > DX_NOTE_FILE_MAX) {
        return;
    }
    n->notes[i].size = (uint32_t)size;
    while (len < size && data[len] != '\n') {
        len++;
    }
    if (len > 0 && data[len - 1] == '\r') {
        len--;
    }
    for (k = 0; k < len; k++) {
        if (data[k] != ' ' && data[k] != '\t') {
            blank = 0;
        }
    }
    if (!blank) {
        dx_str_cut(n->notes[i].title, DX_NOTE_TITLE_MAX + 1, (const char *)data, len);
    }
}

void dx_notes_finish(dx_notes_t *n)
{
    int i;
    int k = 0;

    for (i = 0; i < n->count; i++) {
        if (n->notes[i].title[0] != '\0') {
            n->notes[k++] = n->notes[i];
        }
    }
    n->count = k;
}

int dx_notes_body(const uint8_t *data, uint32_t size, char *out, int cap)
{
    uint32_t i = 0;
    int len = 0;

    if (cap <= 0) {
        return 0;
    }
    while (i < size && data[i] != '\n') {
        i++;
    }
    for (i++; i < size && len < cap - 1; i++) {
        uint8_t c = data[i];

        if (c == '\r' && i + 1 < size && data[i + 1] == '\n') {
            continue;
        }
        if ((c < 0x20 && c != '\t' && c != '\n') || c == 0x7f) {
            c = '?';
        }
        out[len++] = (char)c;
    }
    out[len] = '\0';
    return len;
}
