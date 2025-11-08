#include "err.h"

#include <unistd.h>

#include <iostream>
#include <vector>

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
#define HAS_CLOSE_RANGE 1
#endif

namespace {
// Closes all open file descriptors except stdin(0), stdout(1), stderr(2).
// Ignores errors because we exit(1) afterwards anyways.
// Should work in mimuw lab environment, macros are for safety.
void cleanup() {
#ifdef HAS_CLOSE_RANGE
    close_range(3, ~0U, 0);
#endif
}
} // namespace

[[noreturn]] void syserr(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, " (%d; %s)\n", org_errno, strerror(org_errno));
    cleanup();
    exit(1);
}

[[noreturn]] void fatal(const char* fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, "\n");
    cleanup();
    exit(1);
}

void error(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    if (org_errno != 0) {
        fprintf(stderr, " (%d; %s)\n", org_errno, strerror(org_errno));
    }
    errno = 0;

    fprintf(stderr, "\n");
}