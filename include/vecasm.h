#ifndef VECASM_H
#define VECASM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(VECASM_SHARED)
#  ifdef VECASM_BUILD
#    define VECASM_API __declspec(dllexport)
#  else
#    define VECASM_API __declspec(dllimport)
#  endif
#else
#  define VECASM_API
#endif

enum {
    VECASM_CAP_SCALAR = 1u << 0,
    VECASM_CAP_AVX2   = 1u << 1,
    VECASM_CAP_FMA    = 1u << 2,
    VECASM_CAP_AVX512 = 1u << 3
};

VECASM_API uint32_t vecasm_caps(void);

/* 0 = auto, 1 = scalar, 2 = avx2. Returns previous mode. */
VECASM_API int vecasm_set_backend(int mode);

/* Dense kernels. Prefer 32-byte aligned pointers for best speed. */
VECASM_API float  vecasm_dot_f32(const float *a, const float *b, size_t n);
VECASM_API double vecasm_dot_f64(const double *a, const double *b, size_t n);
VECASM_API void   vecasm_axpy_f32(float alpha, const float *x, float *y, size_t n);
VECASM_API float  vecasm_sum_f32(const float *a, size_t n);
VECASM_API float  vecasm_norm2_f32(const float *a, size_t n);

/* Tight vec3 (xyz). Arrays of 3 floats. */
VECASM_API float vecasm_dot3_f32(const float a[3], const float b[3]);
VECASM_API void  vecasm_cross3_f32(const float a[3], const float b[3], float out[3]);
/* If out == in, normalizes in place. Zero vector -> out = {0,0,0}. */
VECASM_API void  vecasm_normalize3_f32(const float in[3], float out[3]);

#ifdef __cplusplus
}
#endif

#endif /* VECASM_H */
