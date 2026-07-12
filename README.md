# vecasm

Hand-written **NASM** dense float kernels with **runtime ISA dispatch** (optional calibration). C API + thin C++ wrapper.

Cross-platform x86_64: Windows (Win64 ABI), Linux/macOS (System V).

## Advantage

1. **Detect** CPU/OS features (CPUID + XGETBV). `avx512icl` only on **GenuineIntel** + FMA + VNNI + VBMI2.
2. **Default AUTO** = highest supported ISA (`icl > avx512 > avx2 > scalar`). Instant, no heap spike.
3. **Optional** `vecasm_calibrate()` builds a `{op,tier}` timing table and switches to `VECASM_DISPATCH_CALIBRATED` (~0.5s / up to ~192 MiB). Never runs by itself.
4. **Hot path**: `vecasm_resolve_*` / `vecasm_resolve_dense` return direct function pointers (no per-call atomics).

```c
vecasm_set_dispatch(VECASM_DISPATCH_ISA); /* default, good for games */

vecasm_dense_fns fn;
vecasm_resolve_dense(&fn, n);
for (;;)
    acc += fn.dot(a, b, n);

/* optional offline tune */
vecasm_calibrate();
```

## API

| Kernel | Meaning |
|--------|---------|
| `vecasm_dot_f32` / `sum` / `axpy` / `norm2` | dense (dispatched each call) |
| `vecasm_resolve_dot/sum/axpy` / `resolve_dense` | bind kernel once |
| `vecasm_dot3_f32` / `cross3` / `normalize3` | vec3 |
| `vecasm_set_dispatch` / `calibrate` | ISA vs calibrated |

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/vecasm_test_dot
./build/vecasm_bench_dot
```

## License

**vecasm Attribution License** (`LICENSE` + `NOTICE`).

Credit: `LucPrusPPi (https://github.com/LucPrusPPi/vecasm)`. Do not hide origin.
