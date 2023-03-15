#pragma once

#define LOG_LEVEL_TRACE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5

// Can be set in e.g. Makefile or environment variables.
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

void log_write(char const* file, u32 line, char const* level, char const* fmt, ...);
#define LOG(_level, ...) log_write(__FILE__, __LINE__, _level, ##__VA_ARGS__)

#if LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE(...) LOG("trace", ##__VA_ARGS__)
#else
// This expansion causes the parameters to be checked at compile-time but not evaluated at runtime.
#define LOG_TRACE(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(...) LOG("debug", ##__VA_ARGS__)
#else
#define LOG_DEBUG(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(...) LOG("info", ##__VA_ARGS__)
#else
#define LOG_INFO(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(...) LOG("warn", ##__VA_ARGS__)
#else
#define LOG_WARN(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(...) LOG("error", ##__VA_ARGS__)
#else
#define LOG_ERROR(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif

#if LOG_LEVEL <= LOG_LEVEL_FATAL
#define LOG_FATAL(...) LOG("fatal", ##__VA_ARGS__)
#else
#define LOG_FATAL(...) (0 ? ((void)(__VA_ARGS__)) : ((void)(0)))
#endif
