#pragma once

void memcpy(char* dest, char const* src, usize len);

// does add a NUL terminator as the string's length may vary.
void u64_to_str(char buf[21], u64 value);
// does not add a NUL terminator as the string will always be 16 characters long.
void u64_to_str_hex(char buf[16], u64 value);
