#pragma once

#ifdef CONFIG_SPIRAM
#define MALLOC(s) heap_caps_malloc(s, MALLOC_CAP_SPIRAM)
#define REALLOC(p, s) heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM)
#define CALLOC(s, t) heap_caps_calloc(s, t, MALLOC_CAP_SPIRAM)
#define FREE(p)                                                                                                                    \
    do {                                                                                                                           \
        if (p) {                                                                                                                   \
            heap_caps_free(p);                                                                                                     \
            (p) = NULL;                                                                                                            \
        }                                                                                                                          \
    } while (0)
#else
#define MALLOC(s) malloc(s)
#define CALLOC(s, t) calloc(s, t)
#define REALLOC(p, s) realloc(p, s)
#define FREE(p)                                                                                                                    \
    do {                                                                                                                           \
        if (p) {                                                                                                                   \
            free(p);                                                                                                               \
            (p) = NULL;                                                                                                            \
        }                                                                                                                          \
    } while (0)
#endif

