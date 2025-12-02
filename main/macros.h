#pragma once


static inline void* _malloc_psram(size_t sz) {
    if (!sz) return NULL;
    void* p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p;
}

static inline void* _calloc_psram(size_t n, size_t sz) {
    if (!n || !sz) return NULL;
    void* p = heap_caps_calloc(n, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p;
}

static inline void* _realloc_psram(void* ptr, size_t sz) {
    void* p = heap_caps_realloc(ptr, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p;
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

// DMA capable
#define MALLOC_DMA(s)    heap_caps_malloc((s), MALLOC_CAP_DMA | MALLOC_CAP_8BIT)
#define CALLOC_DMA(n, s) heap_caps_calloc((n), (s), MALLOC_CAP_DMA | MALLOC_CAP_8BIT)

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