/*
 * Checked formatting for the host tests. tfmt() writes like snprintf()
 * but stops the test run with a message when the output does not fit.
 * It is the only place in tests/host/ that calls the libc formatters:
 * tests/host/check_format_calls.py fails on any other call. CI builds
 * with GCC, whose -Wformat-truncation flags unchecked snprintf calls
 * that Apple clang lets through (docs/TESTING.md).
 */
#ifndef DX_TFMT_H
#define DX_TFMT_H

#include <stdarg.h>
#include <stddef.h>

/* Returns the length written; never truncates. */
int tfmt(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int tvfmt(char *buf, size_t size, const char *fmt, va_list ap) __attribute__((format(printf, 3, 0)));

#endif
