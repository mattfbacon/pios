#pragma once

#include <stddef.h>

void* malloc(size_t size);
void free(void* address);
void* calloc(size_t num_members, size_t size);
void* realloc(void* old_address, size_t size);
#define alloca __builtin_alloca

size_t malloc_usable_size(void* address);
