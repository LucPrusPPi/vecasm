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

/* Runtime capability flags (bitmask). Detected once via CPUID + XGETBV. */
enum {
    VECASM_CAP_SCALAR     = 1u << 0,
    VECASM_CAP_AVX2       = 1u << 1,
    VECASM_CAP_FMA        = 1u << 2,
    VECASM_CAP_AVX512     = 1u << 3, /* AVX-512F + OS ZMM state */
    /* Intel Ice Lake / Ice Lake-SP only: FMA + VNNI + VBMI2 (not AMD with same bits). */
    VECASM_CAP_AVX512_ICL = 1u << 4
};

enum {
    VECASM_BACKEND_AUTO       = 0,
    VECASM_BACKEND_SCALAR     = 1,
    VECASM_BACKEND_AVX2       = 2,
    VECASM_BACKEND_AVX512     = 3,
    VECASM_BACKEND_AVX512_ICL = 4
};

VECASM_API uint32_t vecasm_caps(void);

/* Force backend. Returns previous mode. Unavailable force falls back at call time. */
VECASM_API int vecasm_set_backend(int mode);

/*
 * Auto path: after micro-benchmark calibration, backend chosen for a mid-size
 * working set. Not a raw ISA priority list.
 */
VECASM_API int vecasm_best_backend(void);

/* Effective backend after force+fallback (mid-size if AUTO). */
VECASM_API int vecasm_active_backend(void);

/* Effective backend for length n (AUTO uses size tiers from calibration). */
VECASM_API int vecasm_active_backend_n(size_t n);

/* Run / re-run timing calibration (also done lazily on first AUTO dense call). */
VECASM_API void vecasm_calibrate(void);

/* Name of a backend id. For AUTO, uses vecasm_best_backend(). */
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
