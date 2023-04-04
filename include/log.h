#pragma once

// We give them a bit of space so we can add more later if necessary.
// Make sure to keep this in sync with `rust/src/panic.rs`!

#define LOG_LEVEL_TRACE 10
#define LOG_LEVEL_DEBUG 20
#define LOG_LEVEL_INFO 30
#define LOG_LEVEL_WARN 40
#define LOG_LEVEL_ERROR 50
#define LOG_LEVEL_FATAL 60

// Can be set in e.g. Makefile or environment variables.
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

void log_write(char const* file, u32 line, u32 level, char const* fmt, ...);
#define LOG(_level, ...) log_write(__FILE__, __LINE__, _level, ##__VA_ARGS__)

#if LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE(...) LOG(LOG_LEVEL_TRACE, ##__VA_ARGS__)
#else
// This expansion causes the parameters to be checked at compile-time but not evaluated at runtime.
#define LOG_TRACE(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(...) LOG(LOG_LEVEL_DEBUG, ##__VA_ARGS__)
#else
#define LOG_DEBUG(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(...) LOG(LOG_LEVEL_INFO, ##__VA_ARGS__)
#else
#define LOG_INFO(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(...) LOG(LOG_LEVEL_WARN, ##__VA_ARGS__)
#else
#define LOG_WARN(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(...) LOG(LOG_LEVEL_ERROR, ##__VA_ARGS__)
#else
#define LOG_ERROR(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_FATAL
#define LOG_FATAL(...) LOG(LOG_LEVEL_FATAL, ##__VA_ARGS__)
#else
#define LOG_FATAL(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif
