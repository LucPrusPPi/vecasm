#include "vecasm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int approx_eq(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0)
        d = -d;
    float s = fabsf(a) + fabsf(b);
    if (s < 1.f)
        s = 1.f;
    return d <= eps * s;
}

static int fail;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                    \
            fail = 1;                                                                          \
        }                                                                                      \
    } while (0)

static void print_caps(uint32_t c)
{
    printf("caps=0x%x\n", c);
    printf("  scalar=%d avx2=%d fma=%d avx512=%d avx512icl=%d\n",
           (c & VECASM_CAP_SCALAR) != 0, (c & VECASM_CAP_AVX2) != 0, (c & VECASM_CAP_FMA) != 0,
           (c & VECASM_CAP_AVX512) != 0, (c & VECASM_CAP_AVX512_ICL) != 0);
}

static void check_backend_vs_scalar(int backend, const float *a, const float *b, const float *y,
                                    size_t n)
{
    float *y0 = (float *)malloc(n * sizeof(float));
    float *y1 = (float *)malloc(n * sizeof(float));
    memcpy(y0, y, n * sizeof(float));
    memcpy(y1, y, n * sizeof(float));

    vecasm_set_backend(VECASM_BACKEND_SCALAR);
    float ds = vecasm_dot_f32(a, b, n);
    float ss = vecasm_sum_f32(a, n);
    vecasm_axpy_f32(1.5f, a, y0, n);

    vecasm_set_backend(backend);
    CHECK(vecasm_active_backend() == backend ||
          /* if somehow unavailable, active must still be a valid fallback */
          vecasm_active_backend() == VECASM_BACKEND_SCALAR ||
          vecasm_active_backend() == VECASM_BACKEND_AVX2 ||
          vecasm_active_backend() == VECASM_BACKEND_AVX512);
    float db = vecasm_dot_f32(a, b, n);
    float sb = vecasm_sum_f32(a, n);
    vecasm_axpy_f32(1.5f, a, y1, n);

    printf("  [%s] active=%s dot %.6f vs scalar %.6f\n", vecasm_backend_name(backend),
           vecasm_backend_name(vecasm_active_backend()), db, ds);
    CHECK(approx_eq(ds, db, 1e-3f));
    CHECK(approx_eq(ss, sb, 1e-3f));
    for (size_t i = 0; i < n; ++i)
        CHECK(approx_eq(y0[i], y1[i], 1e-3f));

    free(y0);
    free(y1);
}

int main(void)
{
    uint32_t caps = vecasm_caps();
    print_caps(caps);
    CHECK(caps & VECASM_CAP_SCALAR);
    CHECK(vecasm_get_dispatch() == VECASM_DISPATCH_ISA);

    /* Default ISA path: no calibrate, still picks a SIMD backend when available. */
    vecasm_set_backend(VECASM_BACKEND_AUTO);
    printf("isa-auto best=%s\n", vecasm_backend_name(vecasm_best_backend()));
    if (caps & VECASM_CAP_AVX2)
        CHECK(vecasm_best_backend() != VECASM_BACKEND_SCALAR);

    vecasm_calibrate();
    CHECK(vecasm_get_dispatch() == VECASM_DISPATCH_CALIBRATED);
    printf("calibrated best(mid dot)=%s\n", vecasm_backend_name(vecasm_best_backend()));
    printf("  n=1K  dot=%s sum=%s axpy=%s\n",
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_DOT, 1024)),
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_SUM, 1024)),
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_AXPY, 1024)));
    printf("  n=1M  dot=%s sum=%s axpy=%s\n",
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_DOT, 1048576)),
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_SUM, 1048576)),
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_AXPY, 1048576)));
    printf("  n=16M dot=%s sum=%s axpy=%s\n",
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_DOT, 1u << 24)),
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_SUM, 1u << 24)),
           vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_AXPY, 1u << 24)));

    /* Back to ISA ignores the timing table. */
    vecasm_set_dispatch(VECASM_DISPATCH_ISA);
    CHECK(vecasm_get_dispatch() == VECASM_DISPATCH_ISA);
    printf("back to isa-auto=%s\n", vecasm_backend_name(vecasm_active_backend()));
    vecasm_set_dispatch(VECASM_DISPATCH_CALIBRATED);

    const size_t n = 1000;
    float *a = (float *)malloc(n * sizeof(float));
    float *b = (float *)malloc(n * sizeof(float));
    float *y = (float *)malloc(n * sizeof(float));
    for (size_t i = 0; i < n; ++i) {
        a[i] = (float)((int)(i % 17) - 8) * 0.25f;
        b[i] = (float)((int)(i % 13) - 6) * 0.5f;
        y[i] = (float)i * 0.01f;
    }

    if (caps & VECASM_CAP_AVX2)
        check_backend_vs_scalar(VECASM_BACKEND_AVX2, a, b, y, n);
    else
        puts("  skip avx2");

    if (caps & VECASM_CAP_AVX512)
        check_backend_vs_scalar(VECASM_BACKEND_AVX512, a, b, y, n);
    else
        puts("  skip avx512");

    if (caps & VECASM_CAP_AVX512_ICL)
        check_backend_vs_scalar(VECASM_BACKEND_AVX512_ICL, a, b, y, n);
    else
        puts("  skip avx512icl (Intel Ice Lake-class only)");

    /* Forced unavailable -> active reflects fallback, not the request. */
    vecasm_set_backend(VECASM_BACKEND_AVX512_ICL);
    int act = vecasm_active_backend();
    printf("force icl -> active=%s\n", vecasm_backend_name(act));
    if (!(caps & VECASM_CAP_AVX512_ICL)) {
        CHECK(act != VECASM_BACKEND_AVX512_ICL);
    }
    float r = vecasm_dot_f32(a, b, n);
    vecasm_set_backend(VECASM_BACKEND_SCALAR);
    CHECK(approx_eq(r, vecasm_dot_f32(a, b, n), 1e-3f));

    {
        const float u[3] = {1.f, 2.f, 3.f};
        const float v[3] = {4.f, -5.f, 6.f};
        float c0[3], n0[3];
        vecasm_set_backend(VECASM_BACKEND_AUTO);
        CHECK(approx_eq(vecasm_dot3_f32(u, v), 12.f, 1e-5f));
        vecasm_cross3_f32(u, v, c0);
        vecasm_normalize3_f32(u, n0);
        CHECK(approx_eq(c0[0], 27.f, 1e-4f));
        CHECK(approx_eq(c0[1], 6.f, 1e-4f));
        CHECK(approx_eq(c0[2], -13.f, 1e-4f));
        float len = sqrtf(n0[0] * n0[0] + n0[1] * n0[1] + n0[2] * n0[2]);
        CHECK(approx_eq(len, 1.f, 1e-4f));
    }

    /* Tiny AUTO / vec3 must not require prior calibrate and stay fast-path capable. */
    {
        vecasm_set_backend(VECASM_BACKEND_AUTO);
        /* Reset calibrate flag indirectly: new process only; here just ensure tiny works. */
        float tiny_a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        float tiny_b[8] = {8, 7, 6, 5, 4, 3, 2, 1};
        float r = vecasm_dot_f32(tiny_a, tiny_b, 8);
        CHECK(approx_eq(r, 120.f, 1e-4f));
    }

    /* Resolve skips per-call atomics in the loop. */
    {
        vecasm_set_backend(VECASM_BACKEND_AUTO);
        vecasm_set_dispatch(VECASM_DISPATCH_ISA);
        vecasm_dense_fns fns;
        vecasm_resolve_dense(&fns, n);
        CHECK(fns.dot != NULL && fns.sum != NULL && fns.axpy != NULL);
        float rd = fns.dot(a, b, n);
        vecasm_set_backend(VECASM_BACKEND_SCALAR);
        CHECK(approx_eq(rd, vecasm_dot_f32(a, b, n), 1e-3f));
        printf("resolve dense backends: dot=%s sum=%s axpy=%s\n",
               vecasm_backend_name(fns.backend_dot), vecasm_backend_name(fns.backend_sum),
               vecasm_backend_name(fns.backend_axpy));
    }

    free(a);
    free(b);
    free(y);

    if (fail) {
        fprintf(stderr, "tests failed\n");
        return 1;
    }
    puts("ok");
    return 0;
}
