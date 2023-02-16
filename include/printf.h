#pragma once

void vdprintf(void (*const write)(char), char const* const fmt, __builtin_va_list args);
