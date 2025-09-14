#pragma once

#ifdef CONFIG_SPIRAM
#define MALLOC(s) heap_caps_malloc(s, MALLOC_CAP_SPIRAM)
#define REALLOC(p, s) heap_caps_realloc(p, s, MALLOC_CAP_SPIRAM)
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