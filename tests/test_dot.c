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

int main(void)
{
    printf("caps=0x%x\n", vecasm_caps());

    const size_t n = 1000;
    float *a = (float *)malloc(n * sizeof(float));
    float *b = (float *)malloc(n * sizeof(float));
    float *y = (float *)malloc(n * sizeof(float));
    for (size_t i = 0; i < n; ++i) {
        a[i] = (float)((int)(i % 17) - 8) * 0.25f;
        b[i] = (float)((int)(i % 13) - 6) * 0.5f;
        y[i] = (float)i * 0.01f;
    }

    vecasm_set_backend(1);
    float ds = vecasm_dot_f32(a, b, n);
    float ss = vecasm_sum_f32(a, n);

    vecasm_set_backend(2);
    float da = vecasm_dot_f32(a, b, n);
    float sa = vecasm_sum_f32(a, n);

    printf("dot scalar=%.6f avx2=%.6f\n", ds, da);
    printf("sum scalar=%.6f avx2=%.6f\n", ss, sa);
    CHECK(approx_eq(ds, da, 1e-4f));
    CHECK(approx_eq(ss, sa, 1e-4f));

    float *y0 = (float *)malloc(n * sizeof(float));
    float *y1 = (float *)malloc(n * sizeof(float));
    memcpy(y0, y, n * sizeof(float));
    memcpy(y1, y, n * sizeof(float));
    vecasm_set_backend(1);
    vecasm_axpy_f32(1.5f, a, y0, n);
    vecasm_set_backend(2);
    vecasm_axpy_f32(1.5f, a, y1, n);
    for (size_t i = 0; i < n; ++i)
        CHECK(approx_eq(y0[i], y1[i], 1e-4f));

    {
        const float u[3] = {1.f, 2.f, 3.f};
        const float v[3] = {4.f, -5.f, 6.f};
        float c0[3], c1[3], n0[3], n1[3];
        vecasm_set_backend(1);
        float d0 = vecasm_dot3_f32(u, v);
        vecasm_cross3_f32(u, v, c0);
        vecasm_normalize3_f32(u, n0);
        vecasm_set_backend(2);
        float d1 = vecasm_dot3_f32(u, v);
        vecasm_cross3_f32(u, v, c1);
        vecasm_normalize3_f32(u, n1);
        CHECK(approx_eq(d0, d1, 1e-5f));
        CHECK(approx_eq(d0, 12.f, 1e-5f)); /* 4-10+18 */
        for (int k = 0; k < 3; ++k) {
            CHECK(approx_eq(c0[k], c1[k], 1e-5f));
            CHECK(approx_eq(n0[k], n1[k], 1e-5f));
        }
        CHECK(approx_eq(c0[0], 27.f, 1e-4f));
        CHECK(approx_eq(c0[1], 6.f, 1e-4f));
        CHECK(approx_eq(c0[2], -13.f, 1e-4f));
    }

    free(a);
    free(b);
    free(y);
    free(y0);
    free(y1);

    if (fail) {
        fprintf(stderr, "tests failed\n");
        return 1;
    }
    puts("ok");
    return 0;
}
