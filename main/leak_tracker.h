#pragma once

void *malloc_wrapper(size_t size, const char *file, int line);
void free_wrapper(void *ptr, const char *file, int line);
char *strdup_wrapper(const char *str, const char *file, int line);

// Conditional macros for memory logging

#define malloc(size) malloc_wrapper(size, __FILE__, __LINE__)
#define free(ptr) free_wrapper(ptr, __FILE__, __LINE__)
#define strdup(str) strdup_wrapper(str, __FILE__, __LINE__)