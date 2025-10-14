// src/runtime/decryptor.c
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #define NOINLINE __declspec(noinline)
  typedef CRITICAL_SECTION obf_mutex_t;
  static void obf_mutex_init(obf_mutex_t *m) { InitializeCriticalSection(m); }
  static void obf_mutex_lock(obf_mutex_t *m) { EnterCriticalSection(m); }
  static void obf_mutex_unlock(obf_mutex_t *m) { LeaveCriticalSection(m); }
#else
  #include <pthread.h>
  #define NOINLINE __attribute__((noinline))
  typedef pthread_mutex_t obf_mutex_t;
  static void obf_mutex_init(obf_mutex_t *m) { pthread_mutex_init(m, NULL); }
  static void obf_mutex_lock(obf_mutex_t *m) { pthread_mutex_lock(m); }
  static void obf_mutex_unlock(obf_mutex_t *m) { pthread_mutex_unlock(m); }
#endif

static void secure_zero(void *p, size_t n) {
    volatile unsigned char *vp = (volatile unsigned char*)p;
    while (n--) *vp++ = 0;
}

static obf_mutex_t obf_mutex;

NOINLINE char *__obf_decrypt(char *enc_ptr, int len, int key) {
    if (len <= 0 || !enc_ptr) return NULL;
    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf) return NULL;
    unsigned char k = (unsigned char)(key & 0xFF);
    obf_mutex_lock(&obf_mutex);
    for (int i = 0; i < len; ++i) {
        volatile unsigned char v = (volatile unsigned char)enc_ptr[i];
        volatile unsigned char d = (volatile unsigned char)(v ^ k);
        buf[i] = (char)d;
    }
    obf_mutex_unlock(&obf_mutex);
    buf[len] = '\0';
    return buf;
}

NOINLINE void __obf_free(char *ptr, int len) {
    if (!ptr) return;
    secure_zero(ptr, (size_t)len);
    free(ptr);
}

NOINLINE int __obf_opaque(int x) {
    volatile int s = x * 1103515245 + 12345;
    s ^= (int)(uintptr_t)(&s);
    s = ((s << 7) | ((unsigned)s >> (25))) ^ (x + ((int)((uintptr_t)&s & 0xFF)));
    return s & 0xFF;
}

/* initializer to set up mutex automatically */
__attribute__((constructor))
static void __obf_runtime_init(void) {
#ifdef _WIN32
    obf_mutex_init(&obf_mutex);
#else
    obf_mutex_init(&obf_mutex);
#endif
}
