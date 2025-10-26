#pragma once
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#ifndef MALLOC_FALLBACK_TO_INTERNAL
#define MALLOC_FALLBACK_TO_INTERNAL 0
#endif

static inline void* _malloc_psram(size_t sz) {
    if (!sz) return NULL;
    void* p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#if MALLOC_FALLBACK_TO_INTERNAL
    if (!p) p = heap_caps_malloc(sz, MALLOC_CAP_8BIT);
#endif
    return p;
}

static inline void* _calloc_psram(size_t n, size_t sz) {
    if (!n || !sz) return NULL;
    void* p = heap_caps_calloc(n, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#if MALLOC_FALLBACK_TO_INTERNAL
    if (!p) p = heap_caps_calloc(n, sz, MALLOC_CAP_8BIT);
#endif
    return p;
}

// heap_caps_realloc() ist „zuverlässig“: bei size==0 → free(NULL), bei Fehler → NULL und alter ptr bleibt gültig
static inline void* _realloc_psram(void* ptr, size_t sz) {
    void* p = heap_caps_realloc(ptr, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#if MALLOC_FALLBACK_TO_INTERNAL
    if (!p && sz) {
        // Optionaler Fallback: neu intern allokieren und kopieren
        size_t old_sz = heap_caps_get_allocated_size(ptr);
        void* np = heap_caps_malloc(sz, MALLOC_CAP_8BIT);
        if (np) {
            size_t tocpy = old_sz < sz ? old_sz : sz;
            memcpy(np, ptr, tocpy);
            heap_caps_free(ptr);
            p = np;
        }
    }
#endif
    return p;  // Bei Fehler (ohne Fallback): NULL, alter ptr bleibt gültig
}

#ifdef CONFIG_SPIRAM
  #define MALLOC(s)      _malloc_psram((s))
  #define CALLOC(n, s)   _calloc_psram((n), (s))
  #define REALLOC(p, s)  _realloc_psram((p), (s))
#else
  #define MALLOC(s)      malloc((s))
  #define CALLOC(n, s)   calloc((n), (s))
  #define REALLOC(p, s)  realloc((p), (s))
#endif

// Für DMA-Puffer (immer intern, DMA-fähig)
#define MALLOC_DMA(s)    heap_caps_malloc((s), MALLOC_CAP_DMA | MALLOC_CAP_8BIT)
#define CALLOC_DMA(n, s) heap_caps_calloc((n), (s), MALLOC_CAP_DMA | MALLOC_CAP_8BIT)

// FREE mit Nullsetzung beibehalten
#define FREE(p) \
    do { if (p) { heap_caps_free((p)); (p) = NULL; } } while (0)


#ifdef __cplusplus
template <typename T>
static void safe_free(T *&ptr)
{
    if (ptr) {
        free(ptr);
        ptr = nullptr;
    }
}

class PThreadGuard {
public:
    explicit PThreadGuard(pthread_mutex_t &m) : m_mutex(m) {
        pthread_mutex_lock(&m_mutex);
    }
    ~PThreadGuard() {
        pthread_mutex_unlock(&m_mutex);
    }
private:
    pthread_mutex_t &m_mutex;
};


#endif