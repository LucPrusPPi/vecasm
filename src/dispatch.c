#include "vecasm.h"

#include <math.h>
#include <stdint.h>

#if defined(_MSC_VER)
#  include <intrin.h>
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
    /* XMM + YMM state enabled by OS */
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

        /* AVX-512F + OS saves opmask/ZMM_Hi256/Hi16_ZMM (bits 5,6,7) */
        int avx512f = (ebx7 & (1 << 16)) != 0;
        int os_zmm = ((xcr0 & 0xE0ull) == 0xE0ull);
        if (avx512f && os_zmm) {
            caps |= VECASM_CAP_AVX512;
            /* Ice Lake / Ice Lake-SP (Xeon Gold 3rd gen): VNNI + VBMI2 */
            int vnni = (ecx7 & (1 << 11)) != 0;
            int vbmi2 = (ecx7 & (1 << 6)) != 0;
            if (vnni && vbmi2)
                caps |= VECASM_CAP_AVX512_ICL;
        }
    }

    g_caps = caps;
    g_inited = 1;
}

uint32_t vecasm_caps(void)
{
    if (!g_inited)
        init_caps();
    return g_caps;
}

int vecasm_best_backend(void)
{
    uint32_t c = vecasm_caps();
    if (c & VECASM_CAP_AVX512_ICL)
        return VECASM_BACKEND_AVX512_ICL;
    if (c & VECASM_CAP_AVX512)
        return VECASM_BACKEND_AVX512;
    if (c & VECASM_CAP_AVX2)
        return VECASM_BACKEND_AVX2;
    return VECASM_BACKEND_SCALAR;
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

/* Resolve effective backend for dense kernels. */
static int dense_backend(void)
{
    if (!g_inited)
        init_caps();

    int want = g_backend;
    if (want == VECASM_BACKEND_AUTO)
        return vecasm_best_backend();

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

float vecasm_dot_f32(const float *a, const float *b, size_t n)
{
    if (n == 0)
        return 0.f;
    switch (dense_backend()) {
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

double vecasm_dot_f64(const double *a, const double *b, size_t n)
{
    return vecasm_dot_f64_scalar(a, b, n);
}

void vecasm_axpy_f32(float alpha, const float *x, float *y, size_t n)
{
    if (n == 0)
        return;
    switch (dense_backend()) {
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
    switch (dense_backend()) {
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

/* vec3 stays on lightweight asm when any SIMD is present. */
static int use_vec3_asm(void)
{
    int b = dense_backend();
    return b != VECASM_BACKEND_SCALAR;
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
