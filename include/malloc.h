#pragma once

void malloc_init(void);

void* malloc(usize size);
void free(void* address);
void* calloc(usize num_members, usize size);
void* realloc(void* old, usize new_size);

usize malloc_usable_size(void* address);
