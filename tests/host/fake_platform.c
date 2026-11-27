#include "fake_platform.h"
#include "tfmt.h"

#include <dirent.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <robco/crc32.h>
#include <robco/ini.h>

fake_platform_t fake;

static void audio_log(int op, uint32_t n, uint32_t first);
static void audio_start(uint32_t ms);

static int live_blocks;

void *fake_alloc(size_t size)
{
    void *p = malloc(size != 0 ? size : 1);

    if (p == NULL) {
        fprintf(stderr, "fake_alloc: out of memory\n");
        abort();
    }
    live_blocks++;
    return p;
}

void fake_free(void *p)
{
    if (p != NULL) {
        live_blocks--;
        free(p);
    }
}

int fake_live_blocks(void)
{
    return live_blocks;
}

/* Every test program links this file: after main returns, release the
 * card and fail the run when a block is still live. */
__attribute__((destructor)) static void fake_check_live(void)
{
    fake_reset();
    if (live_blocks != 0) {
        printf("fake: %d block(s) still live at exit\n", live_blocks);
        fflush(stdout);
        _exit(1);
    }
    printf("fake: no block live at exit\n");
}

void fake_reset(void)
{
    for (int i = 0; i < fake.file_count; i++) {
        fake_free(fake.files[i].owned);
    }
    memset(&fake, 0, sizeof(fake));
    fake.fg = 0x33e066u;
    fake.theme = dx_theme_from_fg(fake.fg);
    fake.ticks = 1000;
    fake.volume = 100;
    fake.rate = DX_RATE_LO;
    for (int i = 0; i < FAKE_HANDLES; i++) {
        fake.handle_file[i] = -1;
    }
}

void fake_push(uint32_t type, uint32_t code, int32_t value)
{
    dx_event_t *ev = &fake.events[fake.event_count++];

    ev->type = type;
    ev->code = code;
    ev->value = value;
}

int fake_hold(uint32_t btn, int repeats)
{
    int i;

    fake_push(DX_EV_BUTTON, btn, DX_PRESS_DOWN);
    for (i = 0; i < repeats; i++) {
        fake_push(DX_EV_BUTTON, btn, DX_PRESS_REPEAT);
    }
    fake_push(DX_EV_BUTTON, btn, DX_PRESS_RELEASE);
    return repeats + 2;
}

void fake_clear_draws(void)
{
    fake.text_count = 0;
    fake.rect_count = 0;
    fake.point_count = 0;
    fake.points_seen = 0;
    fake.presents = 0;
    fake.clears = 0;
    fake.draws[0] = '\0';
}

const fake_text_t *fake_find_text(const char *text)
{
    int i;

    for (i = 0; i < fake.text_count; i++) {
        if (strcmp(fake.texts[i].text, text) == 0) {
            return &fake.texts[i];
        }
    }
    return NULL;
}

int fake_points_inside(dx_rect_t r)
{
    int i;

    for (i = 0; i < fake.point_count; i++) {
        const fake_point_t *p = &fake.points[i];

        if (p->x < r.x || p->y < r.y || p->x >= r.x + r.w || p->y >= r.y + r.h) {
            return 0;
        }
    }
    return 1;
}

int fake_all_points_inside(dx_rect_t r)
{
    if (fake.points_seen == 0) {
        return 1;
    }
    return fake.points_lo.x >= r.x && fake.points_lo.y >= r.y && fake.points_hi.x < r.x + r.w &&
           fake.points_hi.y < r.y + r.h;
}

static void point(int x, int y)
{
    if (fake.points_seen == 0) {
        fake.points_lo.x = fake.points_hi.x = x;
        fake.points_lo.y = fake.points_hi.y = y;
    }
    fake.points_lo.x = x < fake.points_lo.x ? x : fake.points_lo.x;
    fake.points_lo.y = y < fake.points_lo.y ? y : fake.points_lo.y;
    fake.points_hi.x = x > fake.points_hi.x ? x : fake.points_hi.x;
    fake.points_hi.y = y > fake.points_hi.y ? y : fake.points_hi.y;
    fake.points_seen++;
    if (fake.point_count < FAKE_MAX_POINTS) {
        fake.points[fake.point_count].x = x;
        fake.points[fake.point_count].y = y;
        fake.point_count++;
    }
}

/* A colour as its theme name when it has one. */
static const char *color_name(uint32_t c, char buf[12])
{
    if (c == DX_COLOR_NONE) {
        return "none";
    }
    if (c == fake.theme.fg) {
        return "fg";
    }
    if (c == fake.theme.dim) {
        return "dim";
    }
    if (c == fake.theme.faint) {
        return "faint";
    }
    if (c == fake.theme.bg) {
        return "bg";
    }
    tfmt(buf, 12, "#%06x", (unsigned)c);
    return buf;
}

static void emit(const char *fmt, ...)
{
    size_t n = strlen(fake.draws);
    va_list ap;

    if (n >= sizeof(fake.draws) - 1) {
        return;
    }
    va_start(ap, fmt);
    tvfmt(fake.draws + n, sizeof(fake.draws) - n, fmt, ap);
    va_end(ap);
    n = strlen(fake.draws);
    if (n < sizeof(fake.draws) - 1) {
        fake.draws[n] = '\n';
        fake.draws[n + 1] = '\0';
    }
}

static void f_clear(uint32_t color)
{
    char b[12];

    fake.clears++;
    emit("clear %s", color_name(color, b));
}

static void f_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    char b[12];

    point(x0, y0);
    point(x1, y1);
    emit("line %d %d %d %d %s", x0, y0, x1, y1, color_name(color, b));
}

static void f_rect(int x, int y, int w, int h, uint32_t color, int fill)
{
    char b[12];

    if (fake.rect_count < FAKE_MAX_RECTS) {
        fake_rect_t *r = &fake.rects[fake.rect_count++];

        r->x = x;
        r->y = y;
        r->w = w;
        r->h = h;
        r->filled = fill;
        r->fg = color;
    }
    point(x, y);
    point(x + w - 1, y + h - 1);
    emit("rect %d %d %d %d %s %s", x, y, w, h, color_name(color, b), fill ? "fill" : "outline");
}

static void f_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale)
{
    char b1[12], b2[12];

    if (fake.text_count < FAKE_MAX_TEXTS) {
        fake_text_t *t = &fake.texts[fake.text_count++];

        t->x = x;
        t->y = y;
        t->scale = scale;
        t->fg = fg;
        t->bg = bg;
        tfmt(t->text, sizeof(t->text), "%s", s);
    }
    point(x, y);
    point(x + (int)strlen(s) * DX_FONT_W * scale - 1, y + DX_FONT_H * scale - 1);
    emit("text %d %d \"%s\" %s %s s%d", x, y, s, color_name(fg, b1), color_name(bg, b2), scale);
}

static void f_polyline(const int16_t *xy, int points, int closed, uint32_t color)
{
    char b[12];
    int i;

    for (i = 0; i < points; i++) {
        point(xy[2 * i], xy[2 * i + 1]);
    }
    emit("polyline %d %s %s", points, closed ? "closed" : "open", color_name(color, b));
}

static void f_arc(int cx, int cy, int r, int start_deg, int end_deg, uint32_t color, int fill)
{
    char b[12];

    point(cx - r, cy - r);
    point(cx + r, cy + r);
    emit("arc %d %d %d %d %d %s %s", cx, cy, r, start_deg, end_deg, color_name(color, b),
         fill ? "fill" : "outline");
}

static void f_blit1(int x, int y, int w, int h, const uint8_t *bits, int stride, uint32_t fg,
                    uint32_t bg)
{
    char b1[12], b2[12];

    (void)bits;
    point(x, y);
    point(x + w - 1, y + h - 1);
    emit("blit1 %d %d %d %d stride %d %s %s", x, y, w, h, stride, color_name(fg, b1),
         color_name(bg, b2));
}

static void f_clip(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) {
        emit("clip off");
    } else {
        emit("clip %d %d %d %d", x, y, w, h);
    }
}

static void f_present(void)
{
    fake.presents++;
    fake.ticks += fake.draw_ms;
    fake.clock += fake.draw_s;
    emit("present");
}

static const dx_gfx_t recorder = {
    f_clear, f_line, f_rect, f_text, f_polyline, f_arc, f_blit1, f_clip, f_present,
};

const dx_gfx_t *dx_plat_gfx(void)
{
    return &recorder;
}

dx_ctx_t fake_ctx(void)
{
    dx_ctx_t c;

    c.gfx = &recorder;
    c.theme = fake.theme;
    c.now_ms = fake.ticks;
    return c;
}

uint32_t dx_plat_fg(void)
{
    return fake.fg;
}

int dx_plat_time(uint32_t *unix_seconds)
{
    *unix_seconds = fake.clock;
    return fake.clock_valid;
}

uint32_t dx_plat_ticks(void)
{
    return fake.ticks;
}

int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms)
{
    fake.waits++;
    fake.last_timeout = timeout_ms;
    fake.ticks += fake.tick_step;
    if (fake.event_next >= fake.event_count) {
        return 0;
    }
    *ev = fake.events[fake.event_next++];
    return 1;
}

void dx_plat_log(const char *text)
{
    tfmt(fake.log + strlen(fake.log), sizeof(fake.log) - strlen(fake.log), "%s", text);
}

void dx_plat_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    audio_log(FAKE_AU_TONE, freq_hz, duration_ms);
    audio_start(duration_ms);
    if (fake.tone_count < FAKE_MAX_TONES) {
        fake.tones[fake.tone_count][0] = freq_hz;
        fake.tones[fake.tone_count][1] = duration_ms;
        fake.tone_count++;
    }
}

/* ---- The card ----------------------------------------------------------- */

static int find_file(const char *path)
{
    int i;

    for (i = 0; i < fake.file_count; i++) {
        if (strcmp(fake.files[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

static fake_file_t *new_file(const char *path)
{
    int i = find_file(path);

    if (i < 0) {
        if (fake.file_count >= FAKE_MAX_FILES) {
            fprintf(stderr, "fake card full\n");
            return NULL;
        }
        i = fake.file_count++;
    } else {
        fake_free(fake.files[i].owned);
    }
    fake.files[i].owned = NULL;
    tfmt(fake.files[i].path, sizeof(fake.files[i].path), "%s", path);
    return &fake.files[i];
}

void fake_add_file(const char *path, const void *data, uint32_t size)
{
    fake_file_t *f = new_file(path);

    if (f != NULL) {
        f->data = data;
        f->size = size;
        f->fill = 0;
    }
}

void fake_add_owned(const char *path, uint8_t *data, uint32_t size)
{
    fake_file_t *f = new_file(path);

    if (f == NULL) {
        fake_free(data);
        return;
    }
    f->data = data;
    f->owned = data;
    f->size = size;
    f->fill = 0;
}

void fake_add_copy(const char *path, const void *data, uint32_t size)
{
    uint8_t *b = fake_alloc(size);

    memcpy(b, data, size);
    fake_add_owned(path, b, size);
}

void fake_add_fill(const char *path, uint32_t size, uint8_t fill)
{
    fake_file_t *f = new_file(path);

    if (f != NULL) {
        f->data = NULL;
        f->size = size;
        f->fill = fill;
    }
}

void fake_add_text(const char *path, const char *text)
{
    fake_add_file(path, text, (uint32_t)strlen(text));
}

int fake_load_tree(const char *dir, const char *prefix)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int count = 0;

    if (d == NULL) {
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        char disk[512];
        char card[FAKE_PATH_MAX];
        FILE *f;
        long size;
        uint8_t *buf;

        if (e->d_name[0] == '.') {
            continue;
        }
        struct stat st;

        tfmt(disk, sizeof(disk), "%s/%s", dir, e->d_name);
        tfmt(card, sizeof(card), "%s/%s", prefix, e->d_name);
        if (stat(disk, &st) != 0) {
            closedir(d);
            return -1;
        }
        if (S_ISDIR(st.st_mode)) {
            int n = fake_load_tree(disk, card);

            if (n < 0) {
                closedir(d);
                return -1;
            }
            count += n;
            continue;
        }
        f = fopen(disk, "rb");
        if (f == NULL) {
            closedir(d);
            return -1;
        }
        size = (long)st.st_size;
        rewind(f);
        buf = fake_alloc((size_t)size + 1);
        if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
            fclose(f);
            closedir(d);
            fake_free(buf);
            return -1;
        }
        fclose(f);
        fake_add_owned(card, buf, (uint32_t)size);
        count++;
    }
    closedir(d);
    return count;
}

void fake_remove_file(const char *path)
{
    int i = find_file(path);

    if (i >= 0) {
        fake_free(fake.files[i].owned);
        fake.files[i] = fake.files[fake.file_count - 1];
        fake.file_count--;
    }
}

static uint8_t file_byte(const fake_file_t *f, uint32_t at)
{
    return f->data != NULL ? f->data[at] : f->fill;
}

int32_t dx_plat_read_file(const char *path, uint8_t *buf, uint32_t limit)
{
    int i = find_file(path);
    uint32_t k;

    if (i < 0) {
        return DX_FILE_MISSING;
    }
    if (fake.read_fail) {
        return DX_FILE_ERROR;
    }
    if (fake.files[i].size > limit) {
        return DX_FILE_TOO_LARGE;
    }
    for (k = 0; k < fake.files[i].size; k++) {
        buf[k] = file_byte(&fake.files[i], k);
    }
    return (int32_t)fake.files[i].size;
}

int32_t dx_plat_file_open(const char *path)
{
    int i = find_file(path);
    int h;

    if (i < 0) {
        return DX_FILE_MISSING;
    }
    for (h = 0; h < FAKE_HANDLES; h++) {
        if (fake.handle_file[h] < 0) {
            fake.handle_file[h] = i;
            fake.handle_pos[h] = 0;
            fake.handles_open++;
            return h;
        }
    }
    return DX_FILE_ERROR;
}

int32_t dx_plat_file_read(int32_t handle, uint8_t *buf, uint32_t size)
{
    const fake_file_t *f;
    uint32_t k;

    if (handle < 0 || handle >= FAKE_HANDLES || fake.handle_file[handle] < 0) {
        return DX_FILE_ERROR;
    }
    if (fake.read_fail) {
        return DX_FILE_ERROR;
    }
    f = &fake.files[fake.handle_file[handle]];
    for (k = 0; k < size && fake.handle_pos[handle] < f->size; k++) {
        buf[k] = file_byte(f, fake.handle_pos[handle]++);
    }
    return (int32_t)k;
}

void dx_plat_file_close(int32_t handle)
{
    if (handle >= 0 && handle < FAKE_HANDLES && fake.handle_file[handle] >= 0) {
        fake.handle_file[handle] = -1;
        fake.handles_open--;
    }
}

/* Entries directly under `path`, in the order the files were added. A
 * folder shows once, at its first file. */
int32_t dx_plat_dir_list(const char *path, dx_dirent_t *out, uint32_t max, uint32_t start)
{
    char seen[FAKE_MAX_FILES][DX_NAME83_SIZE];
    size_t plen = strlen(path);
    uint32_t index = 0;
    int32_t n = 0;
    int nseen = 0;
    int exists = 0;
    int i;

    if (fake.read_fail) {
        return DX_FILE_ERROR;
    }
    if (fake.dir_page > 0 && max > (uint32_t)fake.dir_page) {
        max = (uint32_t)fake.dir_page;
    }
    for (i = 0; i < fake.file_count; i++) {
        const char *p = fake.files[i].path;
        const char *name;
        const char *slash;
        size_t len;
        int dup = 0;
        int k;

        if (strncmp(p, path, plen) != 0 || p[plen] != '/') {
            continue;
        }
        exists = 1;
        name = p + plen + 1;
        slash = strchr(name, '/');
        len = slash != NULL ? (size_t)(slash - name) : strlen(name);
        if (len == 0 || len >= DX_NAME83_SIZE) {
            continue;
        }
        for (k = 0; k < nseen; k++) {
            if (strncmp(seen[k], name, len) == 0 && seen[k][len] == '\0') {
                dup = 1;
            }
        }
        if (dup) {
            continue;
        }
        memcpy(seen[nseen], name, len);
        seen[nseen][len] = '\0';
        nseen++;
        if (index++ < start || (uint32_t)n >= max) {
            continue;
        }
        memcpy(out[n].name, name, len);
        out[n].name[len] = '\0';
        out[n].is_dir = slash != NULL;
        out[n].size = slash != NULL ? 0 : fake.files[i].size;
        n++;
    }
    return exists ? n : DX_FILE_MISSING;
}

/* ---- Saves -------------------------------------------------------------- */

fake_save_t *fake_find_save(const char *name)
{
    int i;

    for (i = 0; i < fake.save_count; i++) {
        if (strcmp(fake.saves[i].name, name) == 0) {
            return &fake.saves[i];
        }
    }
    return NULL;
}

void fake_put_save(const char *name, const void *data, uint32_t size)
{
    fake_save_t *s = fake_find_save(name);

    if (s == NULL) {
        s = &fake.saves[fake.save_count++];
        tfmt(s->name, sizeof(s->name), "%s", name);
    }
    if (size > FAKE_SAVE_MAX) {
        size = FAKE_SAVE_MAX;
    }
    memcpy(s->data, data, size);
    s->size = size;
}

int32_t dx_plat_save_read(const char *name, uint8_t *buf, uint32_t size)
{
    const fake_save_t *s = fake_find_save(name);

    if (s == NULL) {
        return DX_FILE_MISSING;
    }
    if (size > s->size) {
        size = s->size;
    }
    memcpy(buf, s->data, size);
    return (int32_t)size;
}

int32_t dx_plat_save_write(const char *name, const uint8_t *buf, uint32_t size)
{
    fake.save_writes++;
    if (fake.save_fail > 0 && (fake.save_fail_only == NULL || strcmp(name, fake.save_fail_only) == 0)) {
        fake.save_fail--;
        return DX_FILE_ERROR;
    }
    fake_put_save(name, buf, size);
    return 0;
}

int32_t dx_plat_save_delete(const char *name)
{
    fake_save_t *s = fake_find_save(name);

    if (s == NULL) {
        return DX_FILE_MISSING;
    }
    *s = fake.saves[--fake.save_count];
    return 0;
}

/* ---- Audio: one voice against fake.ticks -------------------------------- */

static void audio_log(int op, uint32_t n, uint32_t first)
{
    if (fake.audio_count < FAKE_MAX_AUDIO) {
        fake_audio_t *a = &fake.audio[fake.audio_count++];

        a->op = op;
        a->n = n;
        a->first = first;
        a->at = fake.ticks;
        a->rate = fake.rate;
    }
}

int fake_audio_ops(int op)
{
    int n = 0;
    int i;

    for (i = 0; i < fake.audio_count; i++) {
        n += fake.audio[i].op == op;
    }
    return n;
}

/* ---- The clock and holotapes ------------------------------------------- */

int32_t dx_plat_set_time(uint32_t unix_seconds)
{
    if (fake.set_time_count < FAKE_MAX_SET_TIMES) {
        fake.set_times[fake.set_time_count] = unix_seconds;
    }
    fake.set_time_count++;
    if (fake.set_time_rc == 0 || fake.set_time_rc == DX_EIO) {
        fake.clock = unix_seconds;
        fake.clock_valid = 1;
    }
    return fake.set_time_rc;
}

void fake_add_holo(const char *file, int kind, const char *title, const uint8_t *icon)
{
    dx_holo_t *h = &fake.holos[fake.holo_count++];

    memset(h, 0, sizeof(*h));
    tfmt(h->file, sizeof(h->file), "%s", file);
    tfmt(h->title, sizeof(h->title), "%s", title);
    h->kind = (uint8_t)kind;
    if (icon != NULL) {
        memcpy(h->icon, icon, sizeof(h->icon));
        for (int i = 0; i < DX_HOLO_ICON_SIZE; i++) {
            h->has_icon |= icon[i] != 0;
        }
    }
}

void fake_add_slot_holo(const char *file, int kind, const char *title)
{
    fake_add_holo(file, kind, title, NULL);
    fake.holos[fake.holo_count - 1].slot = 1;
}

uint32_t dx_plat_holo_events(void)
{
    uint32_t bits = fake.holo_events;

    fake.holo_event_calls++;
    fake.holo_events = 0;
    return bits;
}

int32_t dx_plat_holo_list(dx_holo_t *out, uint32_t max)
{
    int n = fake.holo_count < (int)max ? fake.holo_count : (int)max;

    fake.holo_lists++;
    if (max == 0 || max > DX_HOLO_MAX) {
        return DX_EINVAL;
    }
    if (fake.holo_list_rc != 0) {
        return fake.holo_list_rc;
    }
    memcpy(out, fake.holos, (size_t)n * sizeof(*out));
    return n;
}

int32_t dx_plat_holo_launch(const char *file, const char *args)
{
    fake.launch_count++;
    tfmt(fake.launch_file, sizeof(fake.launch_file), "%s", file);
    tfmt(fake.launch_args, sizeof(fake.launch_args), "%s", args);
    fake.launch_audio_at = fake.audio_count;
    fake.launch_stops_at = fake_audio_ops(FAKE_AU_STOP);
    fake.launch_writes_at = fake.save_writes;
    /* The kernel stops the sound when a tape starts. */
    fake.au_on = 0;
    fake.au_wait = 0;
    fake.ticks += fake.launch_draw_ms;
    fake.holo_events |= fake.launch_events;
    return fake.launch_rc;
}

/* Moves the voice to fake.ticks: a waiting sample takes over at the end. */
static void audio_update(void)
{
    if (!fake.au_on) {
        return;
    }
    while (dx_ticks_due(fake.ticks, fake.au_end)) {
        if (fake.au_wait == 0) {
            fake.au_on = 0;
            return;
        }
        fake.au_end = (fake.au_end + fake.au_wait) & DX_TICKS_MASK;
        fake.au_wait = 0;
    }
}

int fake_audio_busy(void)
{
    audio_update();
    return fake.au_on;
}

static void audio_start(uint32_t ms)
{
    fake.au_on = 1;
    fake.au_wait = 0;
    fake.au_end = (fake.ticks + (ms > 0 ? ms : 1)) & DX_TICKS_MASK;
}

static void take_bytes(const uint8_t *data, uint32_t length)
{
    fake.au_crc = crc32_update(fake.au_crc, data, length);
    fake.au_bytes += length;
}

int32_t dx_plat_play_seq(const dx_note_t *notes, uint32_t count)
{
    uint32_t ms = 0;
    uint32_t i;

    if (count < 1 || count > 256) {
        return -22;
    }
    for (i = 0; i < count; i++) {
        ms += notes[i].duration_ms;
    }
    audio_log(FAKE_AU_SEQ, count, notes[0].freq_hz);
    fake.seq_count++;
    fake.seq_ms = ms;
    audio_start(ms);
    return 0;
}

int32_t dx_plat_sample(const uint8_t *data, uint32_t length)
{
    if (length < 1 || length > DX_SAMPLE_MAX) {
        return -22;
    }
    audio_log(FAKE_AU_SAMPLE, length, data[0]);
    fake.au_starts++;
    fake.au_rate = fake.rate;
    take_bytes(data, length);
    audio_start(length * 1000u / fake.rate);
    return 0;
}

int32_t dx_plat_sample_queue(const uint8_t *data, uint32_t length)
{
    if (length < 1 || length > DX_SAMPLE_MAX) {
        return -22;
    }
    audio_update();
    if (!fake.au_on) {
        audio_log(FAKE_AU_QUEUE, length, data[0]);
        fake.au_starts++;
        fake.au_rate = fake.rate;
        take_bytes(data, length);
        audio_start(length * 1000u / fake.rate);
        return 0;
    }
    if (fake.au_wait != 0) {
        audio_log(FAKE_AU_BUSY, length, data[0]);
        return DX_EBUSY;
    }
    {
        int before = fake.audio_count;
        /* The rate of the play it joins. A tone or a sequence has none:
         * 8000, as before the rate existed. */
        uint32_t rate = fake.au_rate != 0 ? fake.au_rate : DX_RATE_LO;
        uint32_t ms = length * 1000u / rate;

        audio_log(FAKE_AU_QUEUE, length, data[0]);
        if (fake.audio_count > before) {
            fake.audio[before].rate = rate;
        }
        fake.au_nexts++;
        take_bytes(data, length);
        fake.au_wait = ms > 0 ? ms : 1;
    }
    return 0;
}

void dx_plat_audio_stop(void)
{
    audio_log(FAKE_AU_STOP, 0, 0);
    fake.au_on = 0;
    fake.au_wait = 0;
}

int dx_plat_audio_status(void)
{
    return fake_audio_busy();
}

int32_t dx_plat_volume(uint32_t level)
{
    uint32_t prev = fake.volume;

    if (level > 100) {
        return -1;
    }
    audio_log(FAKE_AU_VOLUME, level, 0);
    fake.volume = level;
    return (int32_t)prev;
}

int32_t dx_plat_sample_rate(uint32_t hz)
{
    uint32_t prev = fake.rate;

    audio_log(FAKE_AU_RATE, hz, 0);
    if (fake.rate_result != 0) {
        return fake.rate_result;
    }
    if (hz != DX_RATE_LO && hz != DX_RATE_HI) {
        return DX_EINVAL;
    }
    fake.rate = hz;
    return (int32_t)prev;
}

int dx_plat_sysinfo(dx_sysinfo_t *out)
{
    if (fake.sysinfo_fail) {
        memset(out, 0, sizeof(*out));
        return -1;
    }
    *out = fake.sysinfo;
    return 0;
}

/* ---- Library code: RobCo-Core's, as libpip wraps it ---------------------- */

int dx_ini_parse(const char *text, uint32_t len, dx_ini_handler_t fn, void *ctx,
                 unsigned *err_line)
{
    return ini_parse(text, len, fn, ctx, err_line);
}

const char *dx_ini_strerror(int code)
{
    return ini_strerror(code);
}

uint32_t dx_crc32(const void *data, uint32_t n)
{
    return crc32(data, n);
}
