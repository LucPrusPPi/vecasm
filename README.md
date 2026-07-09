# vecasm

Hand-written **NASM** dense float kernels with **runtime dispatch + micro-benchmark calibration**. C API + thin C++ wrapper.

Cross-platform x86_64: Windows (Win64 ABI), Linux/macOS (System V).

## Advantage: pick what is actually fast on this machine

1. **Detect** what the CPU/OS can run (CPUID + XGETBV).
2. **Calibrate** (lazy on first AUTO call, or `vecasm_calibrate()`): time available backends on small / mid / large `dot` sizes and store winners per size tier.
3. **Dispatch** AUTO calls by length: not a blind ISA ladder.

| Backend | Capability gate |
|---------|-----------------|
| `scalar` | always |
| `avx2` | AVX2 + OS YMM |
| `avx512` | AVX-512F + OS ZMM |
| `avx512icl` | **GenuineIntel** + FMA + VNNI + VBMI2 (Ice Lake / Xeon Gold 3rd gen). AMD with the same bits is **not** tagged ICL. |

Force with `vecasm_set_backend`. Unavailable force falls back safely.

Inspect:

- `vecasm_caps`
- `vecasm_best_backend` (calibrated mid-size winner)
- `vecasm_active_backend` / `vecasm_active_backend_n` (effective after fallback / by `n`)
- `vecasm_backend_name`

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
vecasm_calibrate();
printf("best=%s active_1M=%s caps=0x%x\n",
       vecasm_backend_name(vecasm_best_backend()),
       vecasm_backend_name(vecasm_active_backend_n(1u<<20)),
       vecasm_caps());
float s = vecasm_dot_f32(a, b, n);
```

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/vecasm_test_dot
./build/vecasm_bench_dot
```

## License

**vecasm Attribution License** (`LICENSE` + `NOTICE`).

Credit required: `LucPrusPPi (https://github.com/LucPrusPPi/vecasm)`. Do not hide origin.
