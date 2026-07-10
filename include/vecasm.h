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
    VECASM_CAP_SCALAR     = 1u << 0,
    VECASM_CAP_AVX2       = 1u << 1,
    VECASM_CAP_FMA        = 1u << 2,
    VECASM_CAP_AVX512     = 1u << 3,
    /* GenuineIntel + FMA + VNNI + VBMI2 only (Ice Lake / Xeon Gold 3rd gen). */
    VECASM_CAP_AVX512_ICL = 1u << 4
};

enum {
    VECASM_BACKEND_AUTO       = 0,
    VECASM_BACKEND_SCALAR     = 1,
    VECASM_BACKEND_AVX2       = 2,
    VECASM_BACKEND_AVX512     = 3,
    VECASM_BACKEND_AVX512_ICL = 4
};

/* Dense op ids for per-operation dispatch tables. */
enum {
    VECASM_OP_DOT  = 0,
    VECASM_OP_SUM  = 1,
    VECASM_OP_AXPY = 2
};

VECASM_API uint32_t vecasm_caps(void);

VECASM_API int vecasm_set_backend(int mode);

/* Calibrated mid-size DOT winner (or ISA fallback before calibrate). */
VECASM_API int vecasm_best_backend(void);

/* Effective backend after force+fallback (DOT mid-tier if AUTO). */
VECASM_API int vecasm_active_backend(void);

/* AUTO: calibrated DOT winner for length n. */
VECASM_API int vecasm_active_backend_n(size_t n);

/* AUTO: calibrated winner for operation + length. */
VECASM_API int vecasm_active_backend_for(int op, size_t n);

/*
 * Micro-benchmark calibration (thread-safe). Also runs lazily on the first
 * AUTO dense call with n >= 256. Tiny calls / vec3 never trigger it.
 */
VECASM_API void vecasm_calibrate(void);

VECASM_API const char *vecasm_backend_name(int mode);

VECASM_API float  vecasm_dot_f32(const float *a, const float *b, size_t n);
VECASM_API double vecasm_dot_f64(const double *a, const double *b, size_t n);
VECASM_API void   vecasm_axpy_f32(float alpha, const float *x, float *y, size_t n);
VECASM_API float  vecasm_sum_f32(const float *a, size_t n);
VECASM_API float  vecasm_norm2_f32(const float *a, size_t n);

VECASM_API float vecasm_dot3_f32(const float a[3], const float b[3]);
VECASM_API void  vecasm_cross3_f32(const float a[3], const float b[3], float out[3]);
VECASM_API void  vecasm_normalize3_f32(const float in[3], float out[3]);

#ifdef __cplusplus
}
#endif

#endif /* VECASM_H */
