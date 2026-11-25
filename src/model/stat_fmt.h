/*
 * Text for the STAT tab's device rows: versions, the SoC, memory, the
 * uptime and the date. Pure code, no C library.
 */
#ifndef DX_MODEL_STAT_FMT_H
#define DX_MODEL_STAT_FMT_H

#include <stdint.h>

/* Every buffer below holds this many bytes: the longest text and a NUL. */
#define DX_STAT_TEXT_SIZE 32

/* The ROM's version, from DX_VERSION, the version the build seals the
 * ROM with. "dev" when the build did not set it. */
const char *dx_stat_rom_version(void);

/* "major.minor.patch" from (major << 16) | (minor << 8) | patch. */
void dx_stat_version3(uint32_t v, char out[DX_STAT_TEXT_SIZE]);

/* "major.minor" from (major << 16) | minor. */
void dx_stat_version2(uint32_t v, char out[DX_STAT_TEXT_SIZE]);

/* "BCM2835" for 0x2835 and the like, "0x" and the hex digits for any
 * other value, "unknown" for 0. */
void dx_stat_soc(uint32_t soc, char out[DX_STAT_TEXT_SIZE]);

/* Bytes as "512 MiB" when a whole number of MiB, else "N KiB", or
 * "N B" below 1 KiB. Rounds down. */
void dx_stat_mem(uint32_t bytes, char out[DX_STAT_TEXT_SIZE]);

/* "Dd hh:mm:ss" for milliseconds, such as "0d 00:01:05". */
void dx_stat_uptime(uint32_t seconds, char out[DX_STAT_TEXT_SIZE]);

/* "YYYY-MM-DD hh:mm:ss UTC" for Unix seconds, "no clock" when !valid. */
void dx_stat_datetime(int valid, uint32_t unix_seconds, char out[DX_STAT_TEXT_SIZE]);

#endif
