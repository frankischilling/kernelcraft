#pragma once
#include <stdarg.h>

typedef enum LogLevel {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG,
} LogLevel;

void log_init(void);
void log_msg(LogLevel lvl, const char* fmt, ...);
void log_vmsg(LogLevel lvl, const char* fmt, va_list ap);

#define KC_INFO(...)  log_msg(LOG_INFO,  __VA_ARGS__)
#define KC_WARN(...)  log_msg(LOG_WARN,  __VA_ARGS__)
#define KC_ERR(...)   log_msg(LOG_ERROR, __VA_ARGS__)

#ifdef DEBUG
#define KC_DBG(...)   log_msg(LOG_DEBUG, __VA_ARGS__)
#else
#define KC_DBG(...)   ((void)0)
#endif

/* Lightweight assert (keeps message even in release if you want) */
void kc_panic(const char* fmt, ...);

#define KC_ASSERT(cond) do { \
    if (!(cond)) { kc_panic("ASSERT FAILED: %s (%s:%d)", #cond, __FILE__, __LINE__); } \
} while (0)
