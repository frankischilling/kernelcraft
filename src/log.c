#include "log.h"
#include <stdio.h>
#include <time.h>

static const char* lvl_name(LogLevel lvl) {
    switch (lvl) {
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_DEBUG: return "DEBUG";
        default:        return "?";
    }
}

void log_init(void) {
    /* no-op for now (hook file logging later) */
}

void log_vmsg(LogLevel lvl, const char* fmt, va_list ap) {
    /* Timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm);

    fprintf(stderr, "[%s.%03ld] %-5s ", buf, ts.tv_nsec / 1000000L, lvl_name(lvl));
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void log_msg(LogLevel lvl, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vmsg(lvl, fmt, ap);
    va_end(ap);
}

void kc_panic(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vmsg(LOG_ERROR, fmt, ap);
    va_end(ap);
    /* hard exit: keep it simple for now */
    fflush(stderr);
    abort();
}
