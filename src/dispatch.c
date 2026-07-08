#include "vecasm.h"

#include <math.h>
#include <stdint.h>

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

float vecasm_dot_f32_avx2(const float *a, const float *b, size_t n);
float vecasm_sum_f32_avx2(const float *a, size_t n);
void  vecasm_axpy_f32_avx2(float alpha, const float *x, float *y, size_t n);
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
static int g_backend; /* 0 auto, 1 scalar, 2 avx2 */
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

static void init_caps(void)
{
    int r[4];
    uint32_t caps = VECASM_CAP_SCALAR;

    cpuid_ex(0, 0, r);
    if (r[0] >= 1) {
        cpuid_ex(1, 0, r);
        if ((r[2] & (1 << 27)) && (r[2] & (1 << 28))) {
            cpuid_ex(7, 0, r);
            if (r[1] & (1 << 5))
                caps |= VECASM_CAP_AVX2;
            cpuid_ex(1, 0, r);
            if (r[2] & (1 << 12))
                caps |= VECASM_CAP_FMA;
            cpuid_ex(7, 0, r);
            if (r[1] & (1 << 16))
                caps |= VECASM_CAP_AVX512;
        }
    }
    g_caps = caps;
    g_inited = 1;
}

static int use_avx2(void)
{
    if (!g_inited)
        init_caps();
    if (g_backend == 1)
        return 0;
    if (g_backend == 2)
        return (g_caps & VECASM_CAP_AVX2) != 0;
    return (g_caps & VECASM_CAP_AVX2) != 0;
}

uint32_t vecasm_caps(void)
{
    if (!g_inited)
        init_caps();
    return g_caps;
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
#if defined(VECASM_HAS_AVX2_ASM)
    if (use_avx2())
        return vecasm_dot_f32_avx2(a, b, n);
#endif
    return vecasm_dot_f32_scalar(a, b, n);
}

double vecasm_dot_f64(const double *a, const double *b, size_t n)
{
    return vecasm_dot_f64_scalar(a, b, n);
}

void vecasm_axpy_f32(float alpha, const float *x, float *y, size_t n)
{
    if (n == 0)
        return;
#if defined(VECASM_HAS_AVX2_ASM)
    if (use_avx2()) {
        vecasm_axpy_f32_avx2(alpha, x, y, n);
        return;
    }
#endif
    vecasm_axpy_f32_scalar(alpha, x, y, n);
}

float vecasm_sum_f32(const float *a, size_t n)
{
    if (n == 0)
        return 0.f;
#if defined(VECASM_HAS_AVX2_ASM)
    if (use_avx2())
        return vecasm_sum_f32_avx2(a, n);
#endif
    return vecasm_sum_f32_scalar(a, n);
}

float vecasm_norm2_f32(const float *a, size_t n)
{
    return sqrtf(vecasm_dot_f32(a, a, n));
}

float vecasm_dot3_f32(const float a[3], const float b[3])
{
#if defined(VECASM_HAS_AVX2_ASM)
    if (use_avx2())
        return vecasm_dot3_f32_asm(a, b);
#endif
    return vecasm_dot3_f32_scalar(a, b);
}

void vecasm_cross3_f32(const float a[3], const float b[3], float out[3])
{
#if defined(VECASM_HAS_AVX2_ASM)
    if (use_avx2()) {
        vecasm_cross3_f32_asm(a, b, out);
        return;
    }
#endif
    vecasm_cross3_f32_scalar(a, b, out);
}

void vecasm_normalize3_f32(const float in[3], float out[3])
{
#if defined(VECASM_HAS_AVX2_ASM)
    if (use_avx2()) {
        vecasm_normalize3_f32_asm(in, out);
        return;
    }
#endif
    vecasm_normalize3_f32_scalar(in, out);
}
