# vecasm

Hand-written **NASM** dense float kernels with **runtime CPU dispatch**. C API + thin C++ wrapper.

Cross-platform x86_64: Windows (Win64 ABI), Linux/macOS (System V).

## Advantage: one binary, best path per CPU

At startup (first call) vecasm probes CPUID + XGETBV and picks the fastest safe backend:

| Backend | When |
|---------|------|
| `scalar` | always |
| `avx2` | AVX2 + OS YMM state |
| `avx512` | AVX-512F + OS ZMM state |
| `avx512icl` | Ice Lake class: AVX-512F + VNNI + VBMI2 (Ice Lake / Xeon Gold 3rd gen) |

Auto priority: **icl > avx512 > avx2 > scalar**. Force with `vecasm_set_backend`. Inspect with `vecasm_caps`, `vecasm_best_backend`, `vecasm_backend_name`.

Unavailable forced backends fall back safely (no illegal instruction).

## API

| Kernel | Meaning |
|--------|---------|
| `vecasm_dot_f32` | sum `a[i]*b[i]` |
| `vecasm_sum_f32` | sum `a[i]` |
| `vecasm_axpy_f32` | `y[i] += alpha * x[i]` |
| `vecasm_norm2_f32` | `sqrt(dot(a,a))` |
| `vecasm_dot3_f32` / `cross3` / `normalize3` | tight vec3 |

```c
#include <vecasm.h>
printf("best=%s caps=0x%x\n", vecasm_backend_name(0), vecasm_caps());
float s = vecasm_dot_f32(a, b, n);
```

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/vecasm_test_dot    # compares every available backend vs scalar
./build/vecasm_bench_dot
```

Tests skip AVX-512 / ICL paths when the host CPU lacks them. To validate those kernels, run the same binary on Ice Lake / Xeon Gold 3rd gen (or an AVX-512 sandbox).

## License

**vecasm Attribution License** (`LICENSE` + `NOTICE`).

Credit required: `LucPrusPPi (https://github.com/LucPrusPPi/vecasm)`. Do not hide origin.
