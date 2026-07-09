#include "vecasm.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <malloc.h>
#else
#  include <time.h>
#endif

float vecasm_dot_f32_avx2(const float *a, const float *b, size_t n);
float vecasm_sum_f32_avx2(const float *a, size_t n);
void  vecasm_axpy_f32_avx2(float alpha, const float *x, float *y, size_t n);

float vecasm_dot_f32_avx512(const float *a, const float *b, size_t n);
float vecasm_sum_f32_avx512(const float *a, size_t n);
void  vecasm_axpy_f32_avx512(float alpha, const float *x, float *y, size_t n);

float vecasm_dot_f32_avx512icl(const float *a, const float *b, size_t n);
float vecasm_sum_f32_avx512icl(const float *a, size_t n);
void  vecasm_axpy_f32_avx512icl(float alpha, const float *x, float *y, size_t n);

float vecasm_dot3_f32_asm(const float a[3], const float b[3]);
void  vecasm_cross3_f32_asm(const float a[3], const float b[3], float out[3]);
void  vecasm_normalize3_f32_asm(const float in[3], float out[3]);

float  vecasm_dot_f32_scalar(const float *a, const float *b, size_t n);
double vecasm_dot_f64_scalar(const double *a, const double *b, size_t n);
void   vecasm_axpy_f32_scalar(float alpha, const float *x, float *y, size_t n);
float  vecasm_sum_f32_scalar(const float *a, size_t n);
float  vecasm_dot3_f32_scalar(const float a[3], const float b[3]);
void   vecasm_cross3_f32_scalar(const float a[3], const float b[3], float out[3]);
void   vecasm_normalize3_f32_scalar(const float in[3], float out[3]);

static uint32_t g_caps;
static int g_backend; /* 0 auto, else forced id */
static int g_inited;
static int g_calibrated;

enum { TIER_S = 0, TIER_M = 1, TIER_L = 2, TIER_COUNT = 3 };
static int g_tier_be[TIER_COUNT];

static void cpuid_ex(int leaf, int sub, int out[4])
{
#if defined(_MSC_VER)
    __cpuidex(out, leaf, sub);
#elif defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("cpuid"
                     : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                     : "a"(leaf), "c"(sub));
#else
    out[0] = out[1] = out[2] = out[3] = 0;
#endif
}

static uint64_t xgetbv0(void)
{
#if defined(_MSC_VER)
    return (uint64_t)_xgetbv(0);
#elif defined(__GNUC__) || defined(__clang__)
    uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((uint64_t)edx << 32) | eax;
#else
    return 0;
#endif
}

static double now_s(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

static void *align64(size_t bytes)
{
#if defined(_WIN32)
    return _aligned_malloc(bytes, 64);
#else
    void *p = NULL;
    if (posix_memalign(&p, 64, bytes) != 0)
        return NULL;
    return p;
#endif
}

static void free64(void *p)
{
#if defined(_WIN32)
    _aligned_free(p);
#else
    free(p);
#endif
}

static int is_genuine_intel(void)
{
    int r[4];
    char vendor[13];
    cpuid_ex(0, 0, r);
    memcpy(vendor + 0, &r[1], 4);
    memcpy(vendor + 4, &r[3], 4);
    memcpy(vendor + 8, &r[2], 4);
    vendor[12] = 0;
    return memcmp(vendor, "GenuineIntel", 12) == 0;
}

static void init_caps(void)
{
    int r[4];
    uint32_t caps = VECASM_CAP_SCALAR;

    cpuid_ex(0, 0, r);
    int max_leaf = r[0];
    if (max_leaf < 1) {
        g_caps = caps;
        g_inited = 1;
        return;
    }

    cpuid_ex(1, 0, r);
    int ecx1 = r[2];
    int osxsave = (ecx1 & (1 << 27)) != 0;
    int avx = (ecx1 & (1 << 28)) != 0;
    int fma = (ecx1 & (1 << 12)) != 0;

    if (!osxsave || !avx) {
        g_caps = caps;
        g_inited = 1;
        return;
    }

    uint64_t xcr0 = xgetbv0();
    if ((xcr0 & 0x6ull) != 0x6ull) {
        g_caps = caps;
        g_inited = 1;
        return;
    }

    if (fma)
        caps |= VECASM_CAP_FMA;

    if (max_leaf >= 7) {
        cpuid_ex(7, 0, r);
        int ebx7 = r[1];
        int ecx7 = r[2];

        if (ebx7 & (1 << 5))
            caps |= VECASM_CAP_AVX2;

        int avx512f = (ebx7 & (1 << 16)) != 0;
        int os_zmm = ((xcr0 & 0xE0ull) == 0xE0ull);
        if (avx512f && os_zmm) {
            caps |= VECASM_CAP_AVX512;
            int vnni = (ecx7 & (1 << 11)) != 0;
            int vbmi2 = (ecx7 & (1 << 6)) != 0;
            /* ICL kernels use FMA; only mark on real Intel Ice Lake-class, not AMD. */
            if (fma && vnni && vbmi2 && is_genuine_intel())
                caps |= VECASM_CAP_AVX512_ICL;
        }
    }

    g_caps = caps;
    g_inited = 1;
}

static int resolve_forced(int want)
{
    uint32_t c = g_caps;
    switch (want) {
    case VECASM_BACKEND_AVX512_ICL:
        if (c & VECASM_CAP_AVX512_ICL)
            return VECASM_BACKEND_AVX512_ICL;
        /* fall through */
    case VECASM_BACKEND_AVX512:
        if (c & VECASM_CAP_AVX512)
            return VECASM_BACKEND_AVX512;
        /* fall through */
    case VECASM_BACKEND_AVX2:
        if (c & VECASM_CAP_AVX2)
            return VECASM_BACKEND_AVX2;
        /* fall through */
    case VECASM_BACKEND_SCALAR:
    default:
        return VECASM_BACKEND_SCALAR;
    }
}

static int isa_fallback_best(void)
{
    uint32_t c = g_caps;
    if (c & VECASM_CAP_AVX512_ICL)
        return VECASM_BACKEND_AVX512_ICL;
    if (c & VECASM_CAP_AVX512)
        return VECASM_BACKEND_AVX512;
    if (c & VECASM_CAP_AVX2)
        return VECASM_BACKEND_AVX2;
    return VECASM_BACKEND_SCALAR;
}

static float dot_raw(int be, const float *a, const float *b, size_t n)
{
    switch (be) {
    case VECASM_BACKEND_AVX512_ICL:
        return vecasm_dot_f32_avx512icl(a, b, n);
    case VECASM_BACKEND_AVX512:
        return vecasm_dot_f32_avx512(a, b, n);
    case VECASM_BACKEND_AVX2:
        return vecasm_dot_f32_avx2(a, b, n);
    default:
        return vecasm_dot_f32_scalar(a, b, n);
    }
}

static void collect_candidates(int *out, int *nout)
{
    uint32_t c = g_caps;
    int n = 0;
    out[n++] = VECASM_BACKEND_SCALAR;
    if (c & VECASM_CAP_AVX2)
        out[n++] = VECASM_BACKEND_AVX2;
    if (c & VECASM_CAP_AVX512)
        out[n++] = VECASM_BACKEND_AVX512;
    if (c & VECASM_CAP_AVX512_ICL)
        out[n++] = VECASM_BACKEND_AVX512_ICL;
    *nout = n;
}

static void run_calibrate(void)
{
    static const size_t tier_n[TIER_COUNT] = {1024u, 65536u, 1048576u};
    static const int tier_iters[TIER_COUNT] = {800, 40, 6};

    int cands[4];
    int nc = 0;
    collect_candidates(cands, &nc);

    /* Default until timing succeeds. */
    int fb = isa_fallback_best();
    for (int t = 0; t < TIER_COUNT; ++t)
        g_tier_be[t] = fb;

    if (nc <= 1) {
        g_calibrated = 1;
        return;
    }

    const size_t max_n = tier_n[TIER_COUNT - 1];
    float *a = (float *)align64(max_n * sizeof(float));
    float *b = (float *)align64(max_n * sizeof(float));
    if (!a || !b) {
        free64(a);
        free64(b);
        g_calibrated = 1;
        return;
    }
    for (size_t i = 0; i < max_n; ++i) {
        a[i] = (float)((i * 13) % 97) * 0.01f + 0.01f;
        b[i] = (float)((i * 7) % 89) * 0.01f + 0.01f;
    }

    for (int t = 0; t < TIER_COUNT; ++t) {
        size_t n = tier_n[t];
        int iters = tier_iters[t];
        double best_s = 1e300;
        int best_be = cands[0];

        for (int ci = 0; ci < nc; ++ci) {
            int be = cands[ci];
            volatile float sink = 0.f;
            /* warmup */
            sink += dot_raw(be, a, b, n);
            double t0 = now_s();
            for (int i = 0; i < iters; ++i)
                sink += dot_raw(be, a, b, n);
            double t1 = now_s();
            double secs = t1 - t0;
            if (secs < best_s) {
                best_s = secs;
                best_be = be;
            }
            (void)sink;
        }
        g_tier_be[t] = best_be;
    }

    free64(a);
    free64(b);
    g_calibrated = 1;
}

static void ensure_calibrated(void)
{
    if (!g_inited)
        init_caps();
    if (!g_calibrated)
        run_calibrate();
}

static int tier_for(size_t n)
{
    if (n <= 4096u)
        return TIER_S;
    if (n <= 262144u)
        return TIER_M;
    return TIER_L;
}

static int dense_backend_n(size_t n)
{
    if (!g_inited)
        init_caps();
    if (g_backend != VECASM_BACKEND_AUTO)
        return resolve_forced(g_backend);
    ensure_calibrated();
    return g_tier_be[tier_for(n)];
}

uint32_t vecasm_caps(void)
{
    if (!g_inited)
        init_caps();
    return g_caps;
}

void vecasm_calibrate(void)
{
    if (!g_inited)
        init_caps();
    g_calibrated = 0;
    run_calibrate();
}

int vecasm_best_backend(void)
{
    ensure_calibrated();
    return g_tier_be[TIER_M];
}

int vecasm_active_backend(void)
{
    if (!g_inited)
        init_caps();
    if (g_backend != VECASM_BACKEND_AUTO)
        return resolve_forced(g_backend);
    ensure_calibrated();
    return g_tier_be[TIER_M];
}

int vecasm_active_backend_n(size_t n)
{
    return dense_backend_n(n);
}

const char *vecasm_backend_name(int mode)
{
    if (mode == VECASM_BACKEND_AUTO)
        mode = vecasm_best_backend();
    switch (mode) {
    case VECASM_BACKEND_SCALAR:
        return "scalar";
    case VECASM_BACKEND_AVX2:
        return "avx2";
    case VECASM_BACKEND_AVX512:
        return "avx512";
    case VECASM_BACKEND_AVX512_ICL:
        return "avx512icl";
    default:
        return "unknown";
    }
}

int vecasm_set_backend(int mode)
{
    int prev = g_backend;
    g_backend = mode;
    return prev;
}

float vecasm_dot_f32(const float *a, const float *b, size_t n)
{
    if (n == 0)
        return 0.f;
    return dot_raw(dense_backend_n(n), a, b, n);
}

double vecasm_dot_f64(const double *a, const double *b, size_t n)
{
    return vecasm_dot_f64_scalar(a, b, n);
}

void vecasm_axpy_f32(float alpha, const float *x, float *y, size_t n)
{
    if (n == 0)
        return;
    switch (dense_backend_n(n)) {
    case VECASM_BACKEND_AVX512_ICL:
        vecasm_axpy_f32_avx512icl(alpha, x, y, n);
        return;
    case VECASM_BACKEND_AVX512:
        vecasm_axpy_f32_avx512(alpha, x, y, n);
        return;
    case VECASM_BACKEND_AVX2:
        vecasm_axpy_f32_avx2(alpha, x, y, n);
        return;
    default:
        vecasm_axpy_f32_scalar(alpha, x, y, n);
        return;
    }
}

float vecasm_sum_f32(const float *a, size_t n)
{
    if (n == 0)
        return 0.f;
    switch (dense_backend_n(n)) {
    case VECASM_BACKEND_AVX512_ICL:
        return vecasm_sum_f32_avx512icl(a, n);
    case VECASM_BACKEND_AVX512:
        return vecasm_sum_f32_avx512(a, n);
    case VECASM_BACKEND_AVX2:
        return vecasm_sum_f32_avx2(a, n);
    default:
        return vecasm_sum_f32_scalar(a, n);
    }
}

float vecasm_norm2_f32(const float *a, size_t n)
{
    return sqrtf(vecasm_dot_f32(a, a, n));
}

static int use_vec3_asm(void)
{
    return vecasm_active_backend() != VECASM_BACKEND_SCALAR;
}

float vecasm_dot3_f32(const float a[3], const float b[3])
{
    if (use_vec3_asm())
        return vecasm_dot3_f32_asm(a, b);
    return vecasm_dot3_f32_scalar(a, b);
}

void vecasm_cross3_f32(const float a[3], const float b[3], float out[3])
{
    if (use_vec3_asm()) {
        vecasm_cross3_f32_asm(a, b, out);
        return;
    }
    vecasm_cross3_f32_scalar(a, b, out);
}

void vecasm_normalize3_f32(const float in[3], float out[3])
{
    if (use_vec3_asm()) {
        vecasm_normalize3_f32_asm(in, out);
        return;
    }
    vecasm_normalize3_f32_scalar(in, out);
}
