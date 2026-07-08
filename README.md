# vecasm

Hand-written **NASM AVX2** dense float kernels. C API + thin C++ wrapper.

Cross-platform x86_64: Windows (Win64 ABI), Linux/macOS (System V). CPUID dispatch, scalar fallback.

Built for maximum throughput on the primitives you actually compose: `dot`, `sum`, `axpy`, `norm2`, and tight `vec3` ops. If you are writing latency-critical geometry (games tools, overlays, custom math stacks, cheats), you get raw kernels instead of a heavy BLAS or a slow C++ vector class. Higher-level logic (angles, cones, filters) stays in your code.

## API

| Kernel | Meaning |
|--------|---------|
| `vecasm_dot_f32` | sum `a[i]*b[i]` (unrolled AVX2) |
| `vecasm_sum_f32` | sum `a[i]` |
| `vecasm_axpy_f32` | `y[i] += alpha * x[i]` |
| `vecasm_norm2_f32` | `sqrt(dot(a,a))` |
| `vecasm_dot3_f32` | 3D dot |
| `vecasm_cross3_f32` | 3D cross |
| `vecasm_normalize3_f32` | unit `vec3` (in-place or out) |
| `vecasm_dot_f64` | scalar for now |

```c
#include <vecasm.h>
float s = vecasm_dot_f32(a, b, n);
float d = vecasm_dot3_f32(u, v);
vecasm_normalize3_f32(dir, dir);
```

## Requirements

- **NASM** (not YASM). Optional portable copy under `tools/nasm/` (gitignored).
- C11 (MSVC / clang / gcc), CMake 3.20+, x86_64

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/vecasm_test_dot
./build/vecasm_bench_dot
```

## License

**vecasm Attribution License** (`LICENSE` + `NOTICE`).

Use freely, but you **must** credit `LucPrusPPi (https://github.com/LucPrusPPi/vecasm)` and must **not** hide the origin.
