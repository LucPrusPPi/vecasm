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
#  include <pthread.h>
#  include <stdatomic.h>
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

enum { OP_COUNT = 3 };
enum { TIER_S = 0, TIER_M = 1, TIER_L = 2, TIER_XL = 3, TIER_XXL = 4, TIER_COUNT = 5 };
enum { CAL_SAMPLES = 5 };

typedef struct vecasm_cal_snap {
    int table[OP_COUNT][TIER_COUNT];
} vecasm_cal_snap;

static uint32_t g_caps;
static int g_caps_ready;
/* Immutable published calibration; NULL until vecasm_calibrate(). */
#if defined(_WIN32)
static volatile PVOID g_snap;
static volatile long g_dispatch; /* VECASM_DISPATCH_* */
#else
static _Atomic(vecasm_cal_snap *) g_snap;
static atomic_int g_dispatch;
#endif

static vecasm_cal_snap *snap_load(void)
{
#if defined(_WIN32)
    return (vecasm_cal_snap *)InterlockedCompareExchangePointer(&g_snap, NULL, NULL);
#else
    return atomic_load_explicit(&g_snap, memory_order_acquire);
#endif
}

/* Publish immutable snapshot. Previous snap is leaked (calibrate is rare; readers may hold it). */
static void snap_publish(vecasm_cal_snap *fresh)
{
#if defined(_WIN32)
    (void)InterlockedExchangePointer(&g_snap, fresh);
#else
    vecasm_cal_snap *old = atomic_exchange_explicit(&g_snap, fresh, memory_order_acq_rel);
    (void)old;
#endif
}

#if defined(_WIN32)
static INIT_ONCE g_caps_once = INIT_ONCE_STATIC_INIT;
static INIT_ONCE g_lock_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_cal_cs;
static volatile long g_backend; /* AUTO=0 */

static BOOL CALLBACK init_lock_cb(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    (void)once;
    (void)param;
    (void)ctx;
    InitializeCriticalSection(&g_cal_cs);
    return TRUE;
}

static void ensure_lock(void)
{
    InitOnceExecuteOnce(&g_lock_once, init_lock_cb, NULL, NULL);
}

static void cal_lock(void)
{
    ensure_lock();
    EnterCriticalSection(&g_cal_cs);
}

static void cal_unlock(void) { LeaveCriticalSection(&g_cal_cs); }

static int backend_load(void) { return (int)InterlockedCompareExchange(&g_backend, 0, 0); }
static void backend_store(int v) { InterlockedExchange(&g_backend, (long)v); }
static int dispatch_load(void) { return (int)InterlockedCompareExchange(&g_dispatch, 0, 0); }
static void dispatch_store(int v) { InterlockedExchange(&g_dispatch, (long)v); }
#else
static pthread_once_t g_caps_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_cal_mu = PTHREAD_MUTEX_INITIALIZER;
static atomic_int g_backend;

static void cal_lock(void) { pthread_mutex_lock(&g_cal_mu); }
static void cal_unlock(void) { pthread_mutex_unlock(&g_cal_mu); }
static int backend_load(void) { return atomic_load_explicit(&g_backend, memory_order_acquire); }
static void backend_store(int v) { atomic_store_explicit(&g_backend, v, memory_order_release); }
static int dispatch_load(void) { return atomic_load_explicit(&g_dispatch, memory_order_acquire); }
static void dispatch_store(int v) { atomic_store_explicit(&g_dispatch, v, memory_order_release); }
#endif

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

static void init_caps_body(void)
{
    int r[4];
    uint32_t caps = VECASM_CAP_SCALAR;

    cpuid_ex(0, 0, r);
    int max_leaf = r[0];
    if (max_leaf < 1)
        goto done;

    cpuid_ex(1, 0, r);
    int ecx1 = r[2];
    if (!(ecx1 & (1 << 27)) || !(ecx1 & (1 << 28)))
        goto done;

    uint64_t xcr0 = xgetbv0();
    if ((xcr0 & 0x6ull) != 0x6ull)
        goto done;

    int fma = (ecx1 & (1 << 12)) != 0;
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
            if (fma && vnni && vbmi2 && is_genuine_intel())
                caps |= VECASM_CAP_AVX512_ICL;
        }
    }

done:
    g_caps = caps;
#if defined(_WIN32)
    MemoryBarrier();
#else
    atomic_thread_fence(memory_order_release);
#endif
    g_caps_ready = 1;
}

#if defined(_WIN32)
static BOOL CALLBACK init_caps_cb(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    (void)once;
    (void)param;
    (void)ctx;
    init_caps_body();
    return TRUE;
}
static void ensure_caps(void) { InitOnceExecuteOnce(&g_caps_once, init_caps_cb, NULL, NULL); }
#else
static void init_caps_once(void) { init_caps_body(); }
static void ensure_caps(void) { pthread_once(&g_caps_once, init_caps_once); }
#endif

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
    default:
        return VECASM_BACKEND_SCALAR;
    }
}

static int tier_for(size_t n)
{
    if (n <= 4096u)
        return TIER_S;
    if (n <= 262144u)
        return TIER_M;
    if (n <= 1500000u)
        return TIER_L;
    if (n <= 6000000u)
        return TIER_XL;
    return TIER_XXL;
}

static void collect_candidates(int *out, int *nout)
{
    uint32_t c = g_caps;
    int n = 0;
    /* Prefer SIMD-only candidates when available so AUTO never picks scalar
       due to timer noise on large tiers. */
    if (c & VECASM_CAP_AVX2)
        out[n++] = VECASM_BACKEND_AVX2;
    if (c & VECASM_CAP_AVX512)
        out[n++] = VECASM_BACKEND_AVX512;
    if (c & VECASM_CAP_AVX512_ICL)
        out[n++] = VECASM_BACKEND_AVX512_ICL;
    if (n == 0)
        out[n++] = VECASM_BACKEND_SCALAR;
    *nout = n;
}

static float run_dot(int be, const float *a, const float *b, size_t n)
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

static float run_sum(int be, const float *a, size_t n)
{
    switch (be) {
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

static void run_axpy(int be, float alpha, const float *x, float *y, size_t n)
{
    switch (be) {
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

static void sort5(double *v)
{
    for (int i = 1; i < CAL_SAMPLES; ++i) {
        double key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            --j;
        }
        v[j + 1] = key;
    }
}

static double median5(double *v)
{
    sort5(v);
    return v[CAL_SAMPLES / 2];
}

static void fill_isa_table(int table[OP_COUNT][TIER_COUNT])
{
    int fb = isa_fallback_best();
    for (int o = 0; o < OP_COUNT; ++o)
        for (int t = 0; t < TIER_COUNT; ++t)
            table[o][t] = fb;
}

static void run_calibrate_unlocked(void)
{
    /* S/M/L/XL/XXL — XXL at 16M so huge working sets get their own pick. */
    static const size_t tier_n_full[TIER_COUNT] = {1024u, 65536u, 1048576u, 4194304u, 16777216u};
    static const int tier_iters_full[TIER_COUNT] = {500, 32, 8, 3, 2};
    size_t tier_n[TIER_COUNT];
    int tier_iters[TIER_COUNT];
    memcpy(tier_n, tier_n_full, sizeof(tier_n));
    memcpy(tier_iters, tier_iters_full, sizeof(tier_iters));

    int local[OP_COUNT][TIER_COUNT];
    fill_isa_table(local);

    int cands[4];
    int nc = 0;
    collect_candidates(cands, &nc);
    if (nc <= 1)
        goto publish;

    size_t max_n = tier_n[TIER_COUNT - 1];
    float *a = (float *)align64(max_n * sizeof(float));
    float *b = (float *)align64(max_n * sizeof(float));
    float *y = (float *)align64(max_n * sizeof(float));
    int tiers = TIER_COUNT;
    if (!a || !b || !y) {
        free64(a);
        free64(b);
        free64(y);
        /* Fall back: skip XXL if 16M alloc fails. */
        max_n = tier_n[TIER_XL];
        tiers = TIER_XL + 1;
        a = (float *)align64(max_n * sizeof(float));
        b = (float *)align64(max_n * sizeof(float));
        y = (float *)align64(max_n * sizeof(float));
        if (!a || !b || !y) {
            free64(a);
            free64(b);
            free64(y);
            goto publish;
        }
    }
    for (size_t i = 0; i < max_n; ++i) {
        a[i] = (float)((i * 13) % 97) * 0.01f + 0.01f;
        b[i] = (float)((i * 7) % 89) * 0.01f + 0.01f;
        y[i] = (float)((i * 3) % 53) * 0.01f;
    }

    for (int t = 0; t < tiers; ++t) {
        size_t n = tier_n[t];
        int iters = tier_iters[t];

        {
            double samples[4][CAL_SAMPLES];
            volatile float sink = 0.f;
            for (int ci = 0; ci < nc; ++ci)
                sink += run_dot(cands[ci], a, b, n);
            for (int s = 0; s < CAL_SAMPLES; ++s) {
                for (int ci = 0; ci < nc; ++ci) {
                    int be = cands[ci];
                    double t0 = now_s();
                    for (int i = 0; i < iters; ++i)
                        sink += run_dot(be, a, b, n);
                    samples[ci][s] = now_s() - t0;
                }
            }
            (void)sink;
            double best = 1e300;
            int win = cands[0];
            for (int ci = 0; ci < nc; ++ci) {
                double med = median5(samples[ci]);
                if (med < best) {
                    best = med;
                    win = cands[ci];
                }
            }
            local[VECASM_OP_DOT][t] = win;
        }

        {
            double samples[4][CAL_SAMPLES];
            volatile float sink = 0.f;
            for (int ci = 0; ci < nc; ++ci)
                sink += run_sum(cands[ci], a, n);
            for (int s = 0; s < CAL_SAMPLES; ++s) {
                for (int ci = 0; ci < nc; ++ci) {
                    int be = cands[ci];
                    double t0 = now_s();
                    for (int i = 0; i < iters; ++i)
                        sink += run_sum(be, a, n);
                    samples[ci][s] = now_s() - t0;
                }
            }
            (void)sink;
            double best = 1e300;
            int win = cands[0];
            for (int ci = 0; ci < nc; ++ci) {
                double med = median5(samples[ci]);
                if (med < best) {
                    best = med;
                    win = cands[ci];
                }
            }
            local[VECASM_OP_SUM][t] = win;
        }

        {
            double samples[4][CAL_SAMPLES];
            for (int ci = 0; ci < nc; ++ci) {
                memcpy(y, b, n * sizeof(float));
                run_axpy(cands[ci], 1.1f, a, y, n);
            }
            for (int s = 0; s < CAL_SAMPLES; ++s) {
                for (int ci = 0; ci < nc; ++ci) {
                    int be = cands[ci];
                    memcpy(y, b, n * sizeof(float));
                    double t0 = now_s();
                    for (int i = 0; i < iters; ++i)
                        run_axpy(be, 1.1f, a, y, n);
                    samples[ci][s] = now_s() - t0;
                }
            }
            double best = 1e300;
            int win = cands[0];
            for (int ci = 0; ci < nc; ++ci) {
                double med = median5(samples[ci]);
                if (med < best) {
                    best = med;
                    win = cands[ci];
                }
            }
            local[VECASM_OP_AXPY][t] = win;
        }
    }

    /* If XXL was skipped, copy XL winners. */
    if (tiers < TIER_COUNT) {
        for (int o = 0; o < OP_COUNT; ++o)
            local[o][TIER_XXL] = local[o][TIER_XL];
    }

    free64(a);
    free64(b);
    free64(y);

publish: {
    vecasm_cal_snap *fresh = (vecasm_cal_snap *)malloc(sizeof(*fresh));
    if (!fresh)
        return;
    memcpy(fresh->table, local, sizeof(local));
    snap_publish(fresh);
}
}

static int table_get(int op, int tier)
{
    const vecasm_cal_snap *s = snap_load();
    if (!s)
        return isa_fallback_best();
    if (op < 0 || op >= OP_COUNT)
        op = VECASM_OP_DOT;
    if (tier < 0 || tier >= TIER_COUNT)
        tier = TIER_M;
    return s->table[op][tier];
}

static int use_calibrated_table(void)
{
    return dispatch_load() == VECASM_DISPATCH_CALIBRATED && snap_load() != NULL;
}

static int dense_backend(int op, size_t n)
{
    ensure_caps();
    int want = backend_load();
    if (want != VECASM_BACKEND_AUTO)
        return resolve_forced(want);

    /* Default: ISA only. Timing table only after explicit calibrate + CALIBRATED mode. */
    if (use_calibrated_table())
        return table_get(op, tier_for(n));
    return isa_fallback_best();
}

uint32_t vecasm_caps(void)
{
    ensure_caps();
    return g_caps;
}

int vecasm_set_dispatch(int mode)
{
    int prev = dispatch_load();
    if (mode != VECASM_DISPATCH_ISA && mode != VECASM_DISPATCH_CALIBRATED)
        mode = VECASM_DISPATCH_ISA;
    /* CALIBRATED without a table behaves like ISA until calibrate runs. */
    dispatch_store(mode);
    return prev;
}

int vecasm_get_dispatch(void) { return dispatch_load(); }

void vecasm_calibrate(void)
{
    ensure_caps();
    cal_lock();
    run_calibrate_unlocked();
    cal_unlock();
    dispatch_store(VECASM_DISPATCH_CALIBRATED);
}

int vecasm_best_backend(void)
{
    ensure_caps();
    if (use_calibrated_table())
        return table_get(VECASM_OP_DOT, TIER_M);
    return isa_fallback_best();
}

int vecasm_active_backend(void)
{
    ensure_caps();
    int want = backend_load();
    if (want != VECASM_BACKEND_AUTO)
        return resolve_forced(want);
    if (use_calibrated_table())
        return table_get(VECASM_OP_DOT, TIER_M);
    return isa_fallback_best();
}

int vecasm_active_backend_n(size_t n)
{
    return dense_backend(VECASM_OP_DOT, n);
}

int vecasm_active_backend_for(int op, size_t n)
{
    return dense_backend(op, n);
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
    int prev = backend_load();
    backend_store(mode);
    return prev;
}

static vecasm_dot_f32_fn pick_dot(int be)
{
    switch (be) {
    case VECASM_BACKEND_AVX512_ICL:
        return vecasm_dot_f32_avx512icl;
    case VECASM_BACKEND_AVX512:
        return vecasm_dot_f32_avx512;
    case VECASM_BACKEND_AVX2:
        return vecasm_dot_f32_avx2;
    default:
        return vecasm_dot_f32_scalar;
    }
}

static vecasm_sum_f32_fn pick_sum(int be)
{
    switch (be) {
    case VECASM_BACKEND_AVX512_ICL:
        return vecasm_sum_f32_avx512icl;
    case VECASM_BACKEND_AVX512:
        return vecasm_sum_f32_avx512;
    case VECASM_BACKEND_AVX2:
        return vecasm_sum_f32_avx2;
    default:
        return vecasm_sum_f32_scalar;
    }
}

static vecasm_axpy_f32_fn pick_axpy(int be)
{
    switch (be) {
    case VECASM_BACKEND_AVX512_ICL:
        return vecasm_axpy_f32_avx512icl;
    case VECASM_BACKEND_AVX512:
        return vecasm_axpy_f32_avx512;
    case VECASM_BACKEND_AVX2:
        return vecasm_axpy_f32_avx2;
    default:
        return vecasm_axpy_f32_scalar;
    }
}

vecasm_dot_f32_fn vecasm_resolve_dot_f32(size_t n)
{
    return pick_dot(dense_backend(VECASM_OP_DOT, n));
}

vecasm_sum_f32_fn vecasm_resolve_sum_f32(size_t n)
{
    return pick_sum(dense_backend(VECASM_OP_SUM, n));
}

vecasm_axpy_f32_fn vecasm_resolve_axpy_f32(size_t n)
{
    return pick_axpy(dense_backend(VECASM_OP_AXPY, n));
}

void vecasm_resolve_dense(vecasm_dense_fns *out, size_t n)
{
    if (!out)
        return;
    out->backend_dot = dense_backend(VECASM_OP_DOT, n);
    out->backend_sum = dense_backend(VECASM_OP_SUM, n);
    out->backend_axpy = dense_backend(VECASM_OP_AXPY, n);
    out->dot = pick_dot(out->backend_dot);
    out->sum = pick_sum(out->backend_sum);
    out->axpy = pick_axpy(out->backend_axpy);
}

float vecasm_dot_f32(const float *a, const float *b, size_t n)
{
    if (n == 0)
        return 0.f;
    return run_dot(dense_backend(VECASM_OP_DOT, n), a, b, n);
}

double vecasm_dot_f64(const double *a, const double *b, size_t n)
{
    return vecasm_dot_f64_scalar(a, b, n);
}

void vecasm_axpy_f32(float alpha, const float *x, float *y, size_t n)
{
    if (n == 0)
        return;
    run_axpy(dense_backend(VECASM_OP_AXPY, n), alpha, x, y, n);
}

float vecasm_sum_f32(const float *a, size_t n)
{
    if (n == 0)
        return 0.f;
    return run_sum(dense_backend(VECASM_OP_SUM, n), a, n);
}

float vecasm_norm2_f32(const float *a, size_t n)
{
    return sqrtf(vecasm_dot_f32(a, a, n));
}

/* vec3: use caps only, never trigger calibration. */
static int use_vec3_asm(void)
{
    ensure_caps();
    int want = backend_load();
    if (want == VECASM_BACKEND_SCALAR)
        return 0;
    if (want != VECASM_BACKEND_AUTO)
        return resolve_forced(want) != VECASM_BACKEND_SCALAR;
    return (g_caps & (VECASM_CAP_AVX2 | VECASM_CAP_AVX512 | VECASM_CAP_AVX512_ICL)) != 0;
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
