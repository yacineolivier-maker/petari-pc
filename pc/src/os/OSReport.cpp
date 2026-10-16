// OSReport/OSPanic for the PC port: print to the console instead of the debugger UART.
#include <dolphin/os.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern "C" {

void OSVReport(const char* fmt, va_list args) {
    std::vfprintf(stdout, fmt, args);
}

void OSReport(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    OSVReport(fmt, args);
    va_end(args);
}

void OSPanic(const char* file, int line, const char* fmt, ...) {
    std::fflush(stdout);
    std::fprintf(stderr, "PANIC at %s:%d: ", file, line);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
    std::abort();
}

}  // extern "C"
