/*
 * The header clock: text and timing. Pure code. Times are the Unix
 * seconds Pip-OS's get_time returns, shown as UTC.
 */
#ifndef DX_MODEL_CLOCK_H
#define DX_MODEL_CLOCK_H

#include <stdint.h>

#define DX_CLOCK_TEXT_SIZE 6 /* "HH:MM" and the NUL */
#define DX_CLOCK_NONE      (-1)

/* The minute of the day, 0 to 1439, or DX_CLOCK_NONE when !valid. */
int32_t dx_clock_minute(int valid, uint32_t unix_seconds);

/* "HH:MM" for a minute of the day, "--:--" for DX_CLOCK_NONE. */
void dx_clock_text(int32_t minute, char out[DX_CLOCK_TEXT_SIZE]);

/*
 * Milliseconds from `unix_seconds` to the start of the next minute. The
 * clock has whole seconds, so the real change can come up to 1 s sooner.
 */
uint32_t dx_clock_wait_ms(uint32_t unix_seconds);

#endif
