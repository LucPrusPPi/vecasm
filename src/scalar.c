#include "vecasm.h"

#include <math.h>

float vecasm_dot_f32_scalar(const float *a, const float *b, size_t n)
{
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i)
        acc += (double)a[i] * (double)b[i];
    return (float)acc;
}

double vecasm_dot_f64_scalar(const double *a, const double *b, size_t n)
{
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i)
        acc += a[i] * b[i];
    return acc;
}

void vecasm_axpy_f32_scalar(float alpha, const float *x, float *y, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        y[i] += alpha * x[i];
}

float vecasm_sum_f32_scalar(const float *a, size_t n)
{
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i)
        acc += (double)a[i];
    return (float)acc;
}

float vecasm_norm2_f32_scalar(const float *a, size_t n)
{
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i)
        acc += (double)a[i] * (double)a[i];
    return (float)sqrt(acc);
}

float vecasm_dot3_f32_scalar(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void vecasm_cross3_f32_scalar(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void vecasm_normalize3_f32_scalar(const float in[3], float out[3])
{
    float len2 = in[0] * in[0] + in[1] * in[1] + in[2] * in[2];
    if (len2 <= 0.f) {
        out[0] = out[1] = out[2] = 0.f;
        return;
    }
    float inv = 1.f / sqrtf(len2);
    out[0] = in[0] * inv;
    out[1] = in[1] * inv;
    out[2] = in[2] * inv;
}
