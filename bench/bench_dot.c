#include "vecasm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <malloc.h>
static double now_s(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
static void *xalign(size_t n, size_t align) { return _aligned_malloc(n, align); }
static void xfree(void *p) { _aligned_free(p); }
#else
#  include <time.h>
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
static void *xalign(size_t n, size_t align)
{
    void *p = NULL;
    if (posix_memalign(&p, align, n) != 0)
        return NULL;
    return p;
}
static void xfree(void *p) { free(p); }
#endif

static float bench_dot(int backend, const float *a, const float *b, size_t n, int iters)
{
    vecasm_set_backend(backend);
    volatile float sink = 0.f;
    double t0 = now_s();
    for (int i = 0; i < iters; ++i)
        sink += vecasm_dot_f32(a, b, n);
    double t1 = now_s();
    double secs = t1 - t0;
    double gflops = (2.0 * (double)n * (double)iters) / secs / 1e9;
    const char *name = backend == 1 ? "scalar" : (backend == 2 ? "avx2" : "auto");
    printf("%-8s  n=%zu  iters=%d  %.3f ms  %.2f GFLOP/s  sink=%.3f\n",
           name, n, iters, secs * 1e3, gflops, (double)sink);
    return sink;
}

int main(int argc, char **argv)
{
    size_t n = 1u << 20;
    int iters = 200;
    if (argc > 1)
        n = (size_t)strtoull(argv[1], NULL, 10);
    if (argc > 2)
        iters = atoi(argv[2]);

    printf("vecasm caps=0x%x\n", vecasm_caps());
    float *a = (float *)xalign(n * sizeof(float), 32);
    float *b = (float *)xalign(n * sizeof(float), 32);
    if (!a || !b) {
        fprintf(stderr, "alloc failed\n");
        return 1;
    }
    for (size_t i = 0; i < n; ++i) {
        a[i] = (float)((i * 13) % 100) * 0.01f;
        b[i] = (float)((i * 7) % 100) * 0.01f;
    }

    bench_dot(1, a, b, n, iters);
    if (vecasm_caps() & VECASM_CAP_AVX2)
        bench_dot(2, a, b, n, iters);
    else
        puts("avx2 not available, skip");

    xfree(a);
    xfree(b);
    return 0;
}
