#ifndef ERR_H
#define ERR_H

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Prints an error message, including message for errno, calls cleanup(), and exits with
// code 1.
[[noreturn]] void syserr(const char* fmt, ...);

// Prints an error message, calls cleanup(), and exits with code 1.
[[noreturn]] void fatal(const char* fmt, ...);

// Prints an error message. Does not exit the program.
// If errno is set, it will be included in the error message and reset to 0.
void error(const char* fmt, ...);

#endif // ERR_H