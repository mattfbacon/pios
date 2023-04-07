#pragma once

#include "halt.h"
#include "log.h"

#define TRY(_inner) \
	if (!(_inner)) { \
		return false; \
	}

#define TRY_MSG(_inner) \
	if (!(_inner)) { \
		LOG_WARN("TRY_MSG condition failed: %s", #_inner); \
		return false; \
	}

#define fail(_fmt, ...) ((void)(LOG_FATAL("failed: " _fmt, ##__VA_ARGS__), halt()))

#define assert(_cond, ...) ((_cond) ? ((void)0) : ((void)fail(__VA_ARGS__)))
