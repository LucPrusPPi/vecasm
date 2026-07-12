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

/* How AUTO picks a backend. Default = ISA (no benchmark). */
enum {
    VECASM_DISPATCH_ISA        = 0, /* highest supported: icl > avx512 > avx2 > scalar */
    VECASM_DISPATCH_CALIBRATED = 1  /* use timing table from vecasm_calibrate() */
};

VECASM_API uint32_t vecasm_caps(void);

VECASM_API int vecasm_set_backend(int mode);

/* Default DISPATCH_ISA. CALIBRATED only after (or once you call) vecasm_calibrate(). */
VECASM_API int vecasm_set_dispatch(int mode);
VECASM_API int vecasm_get_dispatch(void);

/* Mid-size DOT winner from calibration table, else ISA pick. */
VECASM_API int vecasm_best_backend(void);

/* Effective backend after force+fallback. */
VECASM_API int vecasm_active_backend(void);

VECASM_API int vecasm_active_backend_n(size_t n);
VECASM_API int vecasm_active_backend_for(int op, size_t n);

/*
 * Optional micro-benchmark (thread-safe, ~0.5s / up to ~192 MiB).
 * Publishes an immutable table and switches dispatch to CALIBRATED.
 * Never runs automatically.
 */
VECASM_API void vecasm_calibrate(void);

VECASM_API const char *vecasm_backend_name(int mode);

/* Direct kernels (skip per-call dispatch). Resolve once per size class / session. */
typedef float (*vecasm_dot_f32_fn)(const float *a, const float *b, size_t n);
typedef float (*vecasm_sum_f32_fn)(const float *a, size_t n);
typedef void (*vecasm_axpy_f32_fn)(float alpha, const float *x, float *y, size_t n);

typedef struct vecasm_dense_fns {
    vecasm_dot_f32_fn dot;
    vecasm_sum_f32_fn sum;
    vecasm_axpy_f32_fn axpy;
    int backend_dot;
    int backend_sum;
    int backend_axpy;
} vecasm_dense_fns;

VECASM_API vecasm_dot_f32_fn  vecasm_resolve_dot_f32(size_t n);
VECASM_API vecasm_sum_f32_fn  vecasm_resolve_sum_f32(size_t n);
VECASM_API vecasm_axpy_f32_fn vecasm_resolve_axpy_f32(size_t n);
VECASM_API void               vecasm_resolve_dense(vecasm_dense_fns *out, size_t n);

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
