# vecasm

Hand-written **NASM** dense float kernels with **thread-safe calibrated dispatch**. C API + thin C++ wrapper.

Cross-platform x86_64: Windows (Win64 ABI), Linux/macOS (System V).

## Advantage

1. **Detect** CPU/OS features (CPUID + XGETBV). `avx512icl` only on **GenuineIntel** + FMA + VNNI + VBMI2.
2. **Calibrate** per `{dot,sum,axpy} x {S,M,L,XL}` with interleaved samples and **median** of 5.
3. **Dispatch** AUTO by operation and length. Tiny calls (`n < 256`) and `vec3` use ISA fallback and **never** trigger the ~tens-of-ms calibrate.

Force with `vecasm_set_backend`. Fallback is safe. Inspect with `vecasm_caps`, `vecasm_active_backend_for(op,n)`, `vecasm_calibrate`.

## API

| Kernel | Meaning |
|--------|---------|
| `vecasm_dot_f32` / `sum` / `axpy` / `norm2` | dense |
| `vecasm_dot3_f32` / `cross3` / `normalize3` | vec3 |
| `vecasm_calibrate` | explicit timing table rebuild (thread-safe) |
| `vecasm_active_backend_for` | effective backend for op + `n` |

```c
vecasm_calibrate(); /* optional; also lazy on first AUTO dense n>=256 */
printf("%s\n", vecasm_backend_name(vecasm_active_backend_for(VECASM_OP_SUM, 1u<<24)));
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

Credit: `LucPrusPPi (https://github.com/LucPrusPPi/vecasm)`. Do not hide origin.
