# vecasm

Hand-written **NASM** dense float kernels with **runtime ISA dispatch** (optional calibration). C API + thin C++ wrapper.

Cross-platform x86_64: Windows (Win64 ABI), Linux/macOS (System V).

## Advantage

1. **Detect** CPU/OS features (CPUID + XGETBV). `avx512icl` only on **GenuineIntel** + FMA + VNNI + VBMI2.
2. **Default AUTO** = highest supported ISA (`icl > avx512 > avx2 > scalar`). Instant, no heap spike.
3. **Optional** `vecasm_calibrate()` builds a `{op,tier}` timing table and switches to `VECASM_DISPATCH_CALIBRATED` (~0.5s / up to ~192 MiB). Never runs by itself.

```c
/* game / cheat / latency path */
vecasm_set_dispatch(VECASM_DISPATCH_ISA); /* default */

/* batch / offline path */
vecasm_calibrate(); /* also sets DISPATCH_CALIBRATED */
```

Force a backend with `vecasm_set_backend`. Fallback is safe.

## API

| Kernel | Meaning |
|--------|---------|
| `vecasm_dot_f32` / `sum` / `axpy` / `norm2` | dense |
| `vecasm_dot3_f32` / `cross3` / `normalize3` | vec3 |
| `vecasm_set_dispatch` / `get_dispatch` | ISA vs calibrated |
| `vecasm_calibrate` | optional timing tune |

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
