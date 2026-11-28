/* The platform layer on Pip-OS: the only file that makes system calls. */
#include <pipos/crc32.h>
#include <pipos/ini.h>
#include <pipos/pip.h>

#include "app.h"
#include "platform/platform.h"

/* The fallback when gfx_info fails, which Pip-OS does not do today. */
#define FALLBACK_FG 0x33e066u

_Static_assert(DX_EV_BUTTON == PIP_EV_BUTTON && DX_EV_DIAL == PIP_EV_DIAL, "event types");
_Static_assert(DX_EV_TAPE == PIP_EV_TAPE, "event types");
_Static_assert(DX_HW_RTC == PIP_HW_RTC && DX_HW_AUDIO == PIP_HW_AUDIO && DX_HW_TAPE == PIP_HW_TAPE,
               "hw_flags");
_Static_assert(DX_HW_AUDIO_MID == PIP_HW_AUDIO_MID, "hw_flags");
_Static_assert(DX_RATE_LO == PIP_SAMPLE_RATE && DX_RATE_HI == PIP_SAMPLE_RATE_HI, "rates");
_Static_assert(DX_HOLO_EV_INSERTED == PIP_HOLO_EV_INSERTED &&
                   DX_HOLO_EV_REMOVED == PIP_HOLO_EV_REMOVED && DX_HOLO_EV_BAD == PIP_HOLO_EV_BAD,
               "slot bits");
_Static_assert(DX_ENODEV == -PIP_ENODEV, "errors");
_Static_assert(DX_BTN_LEFT == PIP_BTN_LEFT && DX_BTN_RIGHT == PIP_BTN_RIGHT, "buttons");
_Static_assert(DX_BTN_SELECT == PIP_BTN_SELECT && DX_BTN_DIAL == PIP_BTN_DIAL, "buttons");
_Static_assert(DX_BTN_BACK == PIP_BTN_BACK, "buttons");
_Static_assert(DX_PRESS_RELEASE == PIP_EV_RELEASE && DX_PRESS_REPEAT == PIP_EV_REPEAT, "press");
_Static_assert(DX_FONT_W == PIP_FONT_W && DX_FONT_H == PIP_FONT_H, "font");
_Static_assert(DX_COLOR_NONE == PIP_COLOR_NONE, "colour none");
_Static_assert(DX_SCALE_MAX == PIP_SCALE_MAX, "text scale");
_Static_assert(sizeof(dx_note_t) == sizeof(pip_note_t), "note layout");
_Static_assert(DX_EBUSY == -PIP_EBUSY, "busy");
_Static_assert(DX_NAME83_SIZE == PIP_NAME83_SIZE, "8.3 name");
_Static_assert(DX_SAMPLE_MAX == PIP_SAMPLE_MAX, "sample size");
_Static_assert(DX_SOUND_RADIO == PIP_SND_RADIO && DX_SOUND_SFX == PIP_SND_SFX &&
                   DX_SOUND_RECORDINGS == PIP_SND_RECORDINGS && DX_SOUND_GAMES == PIP_SND_GAMES,
               "sound kinds");

/* 1 when Pip-OS has ABI 1.6 or later, so sound_kind and sound_level
 * exist. Set once in pip_main. */
static int has_levels;

/*
 * Sets the kind of the next play and returns the kind before, or a
 * negative value when nothing was set: before ABI 1.6, or when the call
 * failed. A play keeps the kind it started with, so kind_back() can set
 * the old kind right after the start. A failed kind call never stops the
 * sound: the sound then plays as the kind in force.
 */
static int32_t kind_set(uint32_t kind)
{
    return has_levels ? pip_sound_kind(kind) : -1;
}

static void kind_back(int32_t prev)
{
    if (prev >= 0) {
        pip_sound_kind((uint32_t)prev);
    }
}

uint32_t dx_plat_fg(void)
{
    pip_gfx_info_t gi;

    return pip_gfx_info(&gi) == 0 ? gi.fg_default : FALLBACK_FG;
}

int dx_plat_time(uint32_t *unix_seconds)
{
    return pip_get_time(unix_seconds) == 0;
}

uint32_t dx_plat_ticks(void)
{
    return (uint32_t)pip_ticks_ms();
}

int dx_plat_wait(dx_event_t *ev, uint32_t timeout_ms)
{
    pip_event_t pe;

    if (timeout_ms != DX_WAIT_FOREVER && timeout_ms > PIP_WAIT_MAX_MS) {
        timeout_ms = PIP_WAIT_MAX_MS;
    }
    if (pip_wait_event(&pe, timeout_ms) != 1) {
        return 0;
    }
    ev->type = pe.type;
    ev->code = pe.code;
    ev->value = pe.value;
    return 1;
}

void dx_plat_log(const char *text)
{
    pip_log(text);
}

void dx_plat_tone(uint32_t kind, uint32_t freq_hz, uint32_t duration_ms)
{
    int32_t prev = kind_set(kind);

    pip_tone(freq_hz, duration_ms);
    kind_back(prev);
}

/* ---- Files ---------------------------------------------------------------- */

static int32_t file_error(int32_t rc)
{
    return rc == -PIP_ENOENT ? DX_FILE_MISSING : DX_FILE_ERROR;
}

int32_t dx_plat_read_file(const char *path, uint8_t *buf, uint32_t limit)
{
    int32_t fd = pip_file_open(path);
    uint32_t got = 0;
    uint8_t extra;
    int32_t n;

    if (fd < 0) {
        return file_error(fd);
    }
    while (got < limit) {
        n = pip_file_read(fd, buf + got, limit - got);
        if (n < 0) {
            pip_file_close(fd);
            return DX_FILE_ERROR;
        }
        if (n == 0) {
            break;
        }
        got += (uint32_t)n;
    }
    /* One byte more means the file is above the limit. */
    n = got == limit ? pip_file_read(fd, &extra, 1) : 0;
    pip_file_close(fd);
    if (n < 0) {
        return DX_FILE_ERROR;
    }
    return n > 0 ? DX_FILE_TOO_LARGE : (int32_t)got;
}

int32_t dx_plat_file_open(const char *path)
{
    int32_t fd = pip_file_open(path);

    return fd < 0 ? file_error(fd) : fd;
}

int32_t dx_plat_file_read(int32_t handle, uint8_t *buf, uint32_t size)
{
    int32_t n = pip_file_read(handle, buf, size);

    return n < 0 ? DX_FILE_ERROR : n;
}

void dx_plat_file_close(int32_t handle)
{
    pip_file_close(handle);
}

int32_t dx_plat_dir_list(const char *path, dx_dirent_t *out, uint32_t max, uint32_t start)
{
    static pip_dirent_t ents[PIP_DIR_LIST_MAX];
    int32_t n;
    int32_t i;
    int j;

    if (max > PIP_DIR_LIST_MAX) {
        max = PIP_DIR_LIST_MAX;
    }
    if (max == 0) {
        return 0;
    }
    n = pip_dir_list(path, ents, max, start);
    if (n < 0) {
        return file_error(n);
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < DX_NAME83_SIZE - 1 && ents[i].name[j] != '\0'; j++) {
            out[i].name[j] = ents[i].name[j];
        }
        out[i].name[j] = '\0';
        out[i].is_dir = (ents[i].flags & PIP_DIRENT_DIR) != 0;
        out[i].size = ents[i].size;
    }
    return n;
}

/* ---- Saves ---------------------------------------------------------------- */

int32_t dx_plat_save_read(const char *name, uint8_t *buf, uint32_t size)
{
    int32_t n = pip_read_save(name, buf, size);

    return n < 0 ? file_error(n) : n;
}

int32_t dx_plat_save_write(const char *name, const uint8_t *buf, uint32_t size)
{
    return pip_write_save(name, buf, size) == (int32_t)size ? 0 : DX_FILE_ERROR;
}

int32_t dx_plat_save_delete(const char *name)
{
    int32_t rc = pip_save_delete(name);

    return rc < 0 ? file_error(rc) : 0;
}

/* ---- Audio ---------------------------------------------------------------- */

int32_t dx_plat_play_seq(uint32_t kind, const dx_note_t *notes, uint32_t count)
{
    int32_t prev = kind_set(kind);
    int32_t rc = pip_play_seq((const pip_note_t *)(const void *)notes, count);

    kind_back(prev);
    return rc;
}

int32_t dx_plat_sample(uint32_t kind, const uint8_t *data, uint32_t length)
{
    int32_t prev = kind_set(kind);
    int32_t rc = pip_sample(data, length);

    kind_back(prev);
    return rc;
}

int32_t dx_plat_sample_queue(uint32_t kind, const uint8_t *data, uint32_t length)
{
    int32_t prev = kind_set(kind);
    int32_t rc = pip_sample_queue(data, length);

    kind_back(prev);
    return rc;
}

void dx_plat_audio_stop(void)
{
    pip_audio_stop();
}

int dx_plat_audio_status(void)
{
    return pip_audio_status() == 1;
}

int32_t dx_plat_volume(uint32_t level)
{
    int32_t prev = pip_volume(level);

    return prev < 0 ? -1 : prev;
}

int32_t dx_plat_master(void)
{
    return has_levels ? pip_sound_level(PIP_LEVEL_MASTER, PIP_LEVEL_ASK) : -1;
}

int32_t dx_plat_sample_rate(uint32_t hz)
{
    return pip_sample_rate(hz);
}

/* ---- System information ----------------------------------------------------- */

int dx_plat_sysinfo(dx_sysinfo_t *out)
{
    pip_sys_info_t si;
    pip_mem_info_t mi;

    out->abi_version = 0;
    out->kernel_version = 0;
    out->board_rev = 0;
    out->soc = 0;
    out->mem_total = 0;
    out->heap_size = 0;
    out->stack_size = 0;
    out->hw_flags = 0;
    if (pip_sys_info(&si) != 0 || pip_mem_info(&mi) != 0) {
        return -1;
    }
    out->abi_version = si.abi_version;
    out->kernel_version = si.pipos_version;
    out->board_rev = si.board_rev;
    out->soc = si.soc;
    out->mem_total = si.mem_size;
    out->heap_size = mi.heap_size;
    out->stack_size = mi.stack_size;
    /* Before ABI 1.3 the field was reserved: 0 says nothing there. */
    out->hw_flags = si.abi_version >= DX_ABI_1_3 ? si.hw_flags : 0;
    return 0;
}

/* ---- The clock and holotapes ------------------------------------------------- */

_Static_assert(DX_EPERM == -PIP_EPERM && DX_ENOENT == -PIP_ENOENT && DX_EIO == -PIP_EIO,
               "errors");
_Static_assert(DX_EFAULT == -PIP_EFAULT && DX_EINVAL == -PIP_EINVAL &&
                   DX_ENOEXEC == -PIP_ENOEXEC, "errors");
_Static_assert(DX_HOLO_GAME == PIP_HOLO_GAME && DX_HOLO_UTILITY == PIP_HOLO_UTILITY &&
                   DX_HOLO_DATA == PIP_HOLO_DATA, "holotape kinds");
_Static_assert(DX_HOLO_MAX == PIP_HOLO_ENUM_MAX, "holotape count");
_Static_assert(DX_HOLO_ICON_SIZE == sizeof(((pip_holo_info_t *)0)->icon), "icon size");
_Static_assert(DX_HOLO_TITLE_MAX + 1 == sizeof(((pip_holo_info_t *)0)->title), "title size");

int32_t dx_plat_set_time(uint32_t unix_seconds)
{
    return pip_set_time(unix_seconds);
}

int32_t dx_plat_holo_list(dx_holo_t *out, uint32_t max)
{
    static pip_holo_info_t info[PIP_HOLO_ENUM_MAX];
    int32_t n, i;
    uint32_t j;

    if (max == 0 || max > PIP_HOLO_ENUM_MAX) {
        return DX_EINVAL;
    }
    n = pip_holo_enumerate(info, max);
    for (i = 0; i < n; i++) {
        dx_holo_t *h = &out[i];

        for (j = 0; j < DX_NAME83_SIZE; j++) {
            h->file[j] = info[i].file[j];
        }
        h->file[DX_NAME83_SIZE - 1] = '\0';
        h->kind = info[i].subtype;
        /* 0 before ABI 1.4, where the field was reserved. */
        h->slot = (info[i].flags & PIP_HOLO_SLOT) != 0;
        for (j = 0; j < DX_HOLO_TITLE_MAX; j++) {
            h->title[j] = info[i].title[j];
        }
        h->title[DX_HOLO_TITLE_MAX] = '\0';
        h->has_icon = 0;
        for (j = 0; j < DX_HOLO_ICON_SIZE; j++) {
            h->icon[j] = info[i].icon[j];
            h->has_icon |= info[i].icon[j] != 0;
        }
    }
    return n;
}

int32_t dx_plat_holo_launch(const char *file, const char *args)
{
    return pip_holo_launch(file, args);
}

uint32_t dx_plat_holo_events(void)
{
    int32_t bits = pip_holo_eject_event();

    /* Bits 3 to 31 are 0; a later minor may add some: ignore them. */
    return bits > 0 ? (uint32_t)bits & (DX_HOLO_EV_INSERTED | DX_HOLO_EV_REMOVED | DX_HOLO_EV_BAD)
                    : 0;
}

/* ---- libpip ------------------------------------------------------------------- */

int dx_ini_parse(const char *text, uint32_t len, dx_ini_handler_t fn, void *ctx,
                 unsigned *err_line)
{
    return pip_ini_parse(text, len, fn, ctx, err_line);
}

const char *dx_ini_strerror(int code)
{
    return pip_ini_strerror(code);
}

uint32_t dx_crc32(const void *data, uint32_t n)
{
    return pip_crc32(data, n);
}

/* ---- Drawing ------------------------------------------------------------ */

static void g_clear(uint32_t color)
{
    pip_clear(color);
}

static void g_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    pip_set_color(color, PIP_COLOR_NONE);
    pip_draw_line(x0, y0, x1, y1);
}

static void g_rect(int x, int y, int w, int h, uint32_t color, int fill)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    pip_set_color(color, PIP_COLOR_NONE);
    pip_rect_xywh(x, y, (uint32_t)w, (uint32_t)h, fill ? PIP_RECT_FILLED : PIP_RECT_OUTLINE);
}

static void g_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale)
{
    if (scale < PIP_SCALE_MIN) {
        scale = PIP_SCALE_MIN;
    } else if (scale > PIP_SCALE_MAX) {
        scale = PIP_SCALE_MAX;
    }
    pip_set_color(fg, bg);
    pip_draw_text(x, y, s, (uint32_t)scale);
}

static void g_polyline(const int16_t *xy, int points, int closed, uint32_t color)
{
    if (points < PIP_POLY_MIN || points > DX_POLY_MAX) {
        return;
    }
    pip_set_color(color, PIP_COLOR_NONE);
    pip_polyline(xy, (uint32_t)points, closed ? 1u : 0u);
}

static void g_arc(int cx, int cy, int r, int start_deg, int end_deg, uint32_t color, int fill)
{
    pip_arc_t a;

    if (r < PIP_ARC_RADIUS_MIN || r > PIP_ARC_RADIUS_MAX || start_deg < 0 ||
        start_deg > PIP_ARC_START_MAX || end_deg < 0 || end_deg > PIP_ARC_END_MAX) {
        return;
    }
    a.cx = (int16_t)cx;
    a.cy = (int16_t)cy;
    a.radius = (uint16_t)r;
    a.start_deg = (uint16_t)start_deg;
    a.end_deg = (uint16_t)end_deg;
    a.fill = fill ? PIP_ARC_FILLED : PIP_ARC_OUTLINE;
    pip_set_color(color, PIP_COLOR_NONE);
    pip_arc(&a);
}

static void g_blit1(int x, int y, int w, int h, const uint8_t *bits, int stride, uint32_t fg,
                    uint32_t bg)
{
    pip_blit_t b;

    if (w < 1 || w > PIP_BLIT_W_MAX || h < 1 || h > PIP_BLIT_H_MAX) {
        return;
    }
    /* blit_1bpp draws set bits only: the background is a rectangle first. */
    if (bg != PIP_COLOR_NONE) {
        g_rect(x, y, w, h, bg, 1);
    }
    b.x = (int16_t)x;
    b.y = (int16_t)y;
    b.w = (uint16_t)w;
    b.h = (uint16_t)h;
    b.stride = (uint16_t)stride;
    b.flags = 0;
    b.data = (uint32_t)(uintptr_t)bits;
    b.color = fg;
    pip_blit_1bpp(&b);
}

static void g_clip(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) {
        pip_set_clip(0, 0, 0);
    } else {
        pip_set_clip_xywh(x, y, (uint32_t)w, (uint32_t)h);
    }
}

static void g_present(void)
{
    pip_present();
}

static const dx_gfx_t gfx = {
    g_clear, g_line, g_rect, g_text, g_polyline, g_arc, g_blit1, g_clip, g_present,
};

const dx_gfx_t *dx_plat_gfx(void)
{
    return &gfx;
}

int pip_main(const pipos_startup_t *startup)
{
    has_levels = startup->abi_version >= DX_ABI_1_6;
    dx_main();
}
