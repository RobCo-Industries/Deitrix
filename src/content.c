#include "content.h"

#include "platform/platform.h"
#include "ui/ui.h"

#define LINE_MAX_LEN 200
#define PATH_MAX_LEN 48

/* Every content file but a PCM track fits in this buffer. */
static uint8_t file_buf[DX_ITEMS_LIMIT];
static uint8_t save_buf[DX_INV_SAVE_MAX + 1];

static dx_profile_t profile;
static dx_items_t items;
static dx_notes_t notes;
static dx_maps_t maps;
static dx_radio_t radio;
static dx_settings_t settings;
static int settings_status = DX_SAVE_MISSING;
static int inv_status = DX_SAVE_MISSING;

const dx_profile_t *dx_content_profile(void) { return &profile; }
const dx_notes_t *dx_content_notes(void) { return &notes; }
const dx_maps_t *dx_content_maps(void) { return &maps; }
const dx_radio_t *dx_content_radio(void) { return &radio; }
dx_items_t *dx_content_items(void) { return &items; }
dx_settings_t *dx_content_settings(void) { return &settings; }
int dx_content_settings_status(void) { return settings_status; }
int dx_content_inv_status(void) { return inv_status; }

static void path(char *out, const char *dir, const char *name)
{
    dx_sb_t sb;

    dx_sb_init(&sb, out, PATH_MAX_LEN);
    dx_sb_str(&sb, DX_DATA_DIR);
    if (dir[0] != '\0') {
        dx_sb_char(&sb, '/');
        dx_sb_str(&sb, dir);
    }
    if (name[0] != '\0') {
        dx_sb_char(&sb, '/');
        dx_sb_str(&sb, name);
    }
}

/* "deitrix: load <what> " then, for a file that loaded, the counts. */
static void log_start(dx_sb_t *sb, char *buf, const char *what)
{
    dx_sb_init(sb, buf, LINE_MAX_LEN);
    dx_sb_str(sb, "deitrix: load ");
    dx_sb_str(sb, what);
    dx_sb_char(sb, ' ');
}

static void log_count(dx_sb_t *sb, const char *key, uint32_t n)
{
    dx_sb_str(sb, key);
    dx_sb_char(sb, '=');
    dx_sb_uint(sb, n);
}

static void log_end(dx_sb_t *sb)
{
    dx_sb_char(sb, '\n');
    dx_plat_log(sb->buf);
}

static int32_t read_data(const char *name, uint32_t limit)
{
    char p[PATH_MAX_LEN];

    path(p, "", name);
    return dx_plat_read_file(p, file_buf, limit);
}

static void load_profile(void)
{
    char line[LINE_MAX_LEN];
    dx_sb_t sb;

    dx_profile_load(&profile, file_buf, read_data("PROFILE.INI", DX_PROFILE_LIMIT));
    log_start(&sb, line, "PROFILE.INI");
    if (profile.load.status == DX_LOAD_OK) {
        log_count(&sb, "gauges", (uint32_t)profile.gauge_count);
        dx_sb_char(&sb, ' ');
        log_count(&sb, "attrs", (uint32_t)profile.attr_count);
        dx_sb_char(&sb, ' ');
        log_count(&sb, "about", (uint32_t)profile.about_count);
    } else {
        dx_load_reason(&profile.load, &sb);
    }
    log_end(&sb);
}

static void load_items(void)
{
    char line[LINE_MAX_LEN];
    dx_sb_t sb;

    dx_items_load(&items, file_buf, read_data("ITEMS.INI", DX_ITEMS_LIMIT));
    log_start(&sb, line, "ITEMS.INI");
    if (items.load.status == DX_LOAD_OK) {
        log_count(&sb, "items", (uint32_t)items.item_count);
        dx_sb_char(&sb, ' ');
        log_count(&sb, "categories", (uint32_t)items.cat_count);
    } else {
        dx_load_reason(&items.load, &sb);
    }
    log_end(&sb);
}

static void load_map(void)
{
    char line[LINE_MAX_LEN];
    dx_sb_t sb;
    uint32_t lines = 0, places = 0;
    int i;

    dx_map_load(&maps, file_buf, read_data("MAP.INI", DX_MAP_LIMIT));
    log_start(&sb, line, "MAP.INI");
    if (maps.load.status == DX_LOAD_OK) {
        for (i = 0; i < maps.count; i++) {
            lines += (uint32_t)maps.maps[i].line_count;
            places += (uint32_t)maps.maps[i].place_count;
        }
        log_count(&sb, "maps", (uint32_t)maps.count);
        dx_sb_char(&sb, ' ');
        log_count(&sb, "lines", lines);
        dx_sb_char(&sb, ' ');
        log_count(&sb, "places", places);
    } else {
        dx_load_reason(&maps.load, &sb);
    }
    log_end(&sb);
}

/* Calls fn for every entry of a folder under /PIPBOY/DATA. Returns the
 * first dx_plat_dir_list error, or 0. */
static int32_t list_dir(const char *dir, void (*fn)(const dx_dirent_t *e, void *ctx), void *ctx)
{
    static dx_dirent_t ents[16];
    char p[PATH_MAX_LEN];
    uint32_t start = 0;
    int32_t n;
    int32_t i;

    path(p, dir, "");
    for (;;) {
        n = dx_plat_dir_list(p, ents, sizeof(ents) / sizeof(ents[0]), start);
        if (n <= 0) {
            return n;
        }
        for (i = 0; i < n; i++) {
            fn(&ents[i], ctx);
        }
        start += (uint32_t)n;
    }
}

static void offer_note(const dx_dirent_t *e, void *ctx)
{
    (void)ctx;
    dx_notes_offer(&notes, e);
}

static void load_notes(void)
{
    char line[LINE_MAX_LEN];
    char p[PATH_MAX_LEN];
    dx_sb_t sb;
    int32_t rc;
    int i;

    dx_notes_begin(&notes);
    rc = list_dir("NOTES", offer_note, NULL);
    for (i = 0; i < notes.count; i++) {
        path(p, "NOTES", notes.notes[i].file);
        dx_notes_title(&notes, i, file_buf, dx_plat_read_file(p, file_buf, DX_NOTE_FILE_MAX));
    }
    dx_notes_finish(&notes);
    if (rc < 0) {
        dx_notes_begin(&notes);
        dx_load_size(&notes.load, rc);
    }
    log_start(&sb, line, "NOTES");
    if (notes.load.status == DX_LOAD_OK) {
        log_count(&sb, "notes", (uint32_t)notes.count);
    } else {
        dx_load_reason(&notes.load, &sb);
    }
    log_end(&sb);
}

static void pcm_size(const dx_dirent_t *e, void *ctx)
{
    int i;

    (void)ctx;
    for (i = 0; i < radio.count; i++) {
        dx_station_t *s = &radio.stations[i];

        if (s->kind == DX_KIND_PCM && !e->is_dir && dx_str_eq(s->file, e->name)) {
            dx_radio_set_pcm(&radio, i, e->size > 0x7fffffffu ? DX_FILE_TOO_LARGE
                                                              : (int32_t)e->size);
        }
    }
}

static void load_radio(void)
{
    char line[LINE_MAX_LEN];
    char p[PATH_MAX_LEN];
    dx_sb_t sb;
    int i;

    dx_radio_load(&radio, file_buf, read_data("RADIO.INI", DX_RADIO_LIMIT));
    log_start(&sb, line, "RADIO.INI");
    if (radio.load.status == DX_LOAD_OK) {
        log_count(&sb, "stations", (uint32_t)radio.count);
    } else {
        dx_load_reason(&radio.load, &sb);
    }
    log_end(&sb);
    /* A PCM station is missing until the folder lists its file. */
    for (i = 0; i < radio.count; i++) {
        if (radio.stations[i].kind == DX_KIND_PCM) {
            dx_radio_set_pcm(&radio, i, DX_FILE_MISSING);
        }
    }
    list_dir("RADIO", pcm_size, NULL);
    for (i = 0; i < radio.count; i++) {
        dx_station_t *s = &radio.stations[i];
        char what[PATH_MAX_LEN];
        dx_sb_t w;

        if (s->kind == DX_KIND_TUNE) {
            path(p, "RADIO", s->file);
            dx_radio_set_tune(&radio, i, file_buf, dx_plat_read_file(p, file_buf, DX_TUN_LIMIT));
        }
        dx_sb_init(&w, what, sizeof(what));
        dx_sb_str(&w, "RADIO/");
        dx_sb_str(&w, s->file);
        log_start(&sb, line, what);
        if (s->playable) {
            if (s->kind == DX_KIND_TUNE) {
                log_count(&sb, "notes", (uint32_t)s->note_count);
                dx_sb_char(&sb, ' ');
                log_count(&sb, "ms", s->tune_ms);
            } else {
                log_count(&sb, "bytes", s->pcm_size);
            }
        } else {
            dx_load_reason(&s->file_load, &sb);
        }
        log_end(&sb);
    }
}

void dx_content_load(void)
{
    load_profile();
    load_items();
    load_notes();
    load_map();
    load_radio();
}

void dx_content_load_saves(uint8_t volume)
{
    char line[LINE_MAX_LEN];
    dx_sb_t sb;
    int32_t n;
    int applied = 0;
    uint32_t changes = settings.changes;

    n = dx_plat_save_read(DX_SETTINGS_NAME, save_buf, DX_SETTINGS_SIZE + 1);
    settings_status = dx_settings_decode(&settings, save_buf, n, volume);
    settings.changes = changes;
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: load DEITRIX.SAV ");
    dx_sb_str(&sb, dx_save_reason(settings_status));
    log_count(&sb, " tab", settings.last_tab);
    log_count(&sb, " volume", settings.volume);
    log_count(&sb, " sound", settings.ui_sound);
    log_count(&sb, " station", settings.last_station);
    log_end(&sb);

    changes = items.changes;
    n = dx_plat_save_read(DX_INV_NAME, save_buf, DX_INV_SAVE_MAX + 1);
    inv_status = dx_inv_decode(&items, save_buf, n, &applied);
    items.changes = changes;
    dx_sb_init(&sb, line, sizeof(line));
    dx_sb_str(&sb, "deitrix: load INV.SAV ");
    dx_sb_str(&sb, dx_save_reason(inv_status));
    if (inv_status == DX_SAVE_OK) {
        log_count(&sb, " records", (uint32_t)applied);
    }
    log_end(&sb);

    dx_plat_volume(settings.volume);
    dx_ui_sound_enable(settings.ui_sound);
}

int dx_content_note_body(int i, char *out, int cap)
{
    char p[PATH_MAX_LEN];
    int32_t n;

    if (i < 0 || i >= notes.count) {
        return -1;
    }
    path(p, "NOTES", notes.notes[i].file);
    n = dx_plat_read_file(p, file_buf, DX_NOTE_FILE_MAX);
    if (n < 0) {
        return -1;
    }
    return dx_notes_body(file_buf, (uint32_t)n, out, cap);
}
