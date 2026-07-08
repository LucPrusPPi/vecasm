; float vecasm_dot_f32_avx512icl(const float* a, const float* b, size_t n)
; Ice Lake / Xeon Gold 3rd gen tuned: FMA + prefetch + 4 ZMM acc.
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_dot_f32_avx512icl

vecasm_dot_f32_avx512icl:
    vxorps  xmm0, xmm0, xmm0
    test    LEN_N, LEN_N
    jz      .done

    vxorps  zmm0, zmm0, zmm0
    vxorps  zmm1, zmm1, zmm1
    vxorps  zmm2, zmm2, zmm2
    vxorps  zmm3, zmm3, zmm3

    mov     rax, LEN_N
    shr     rax, 6
    jz      .tail16

.loop64:
    prefetchnta [PTR_A + 512]
    prefetchnta [PTR_B + 512]

    vmovups zmm16, [PTR_A]
    vfmadd231ps zmm0, zmm16, [PTR_B]

    vmovups zmm16, [PTR_A + 64]
    vfmadd231ps zmm1, zmm16, [PTR_B + 64]

    vmovups zmm16, [PTR_A + 128]
    vfmadd231ps zmm2, zmm16, [PTR_B + 128]

    vmovups zmm16, [PTR_A + 192]
    vfmadd231ps zmm3, zmm16, [PTR_B + 192]

    add     PTR_A, 256
    add     PTR_B, 256
    dec     rax
    jnz     .loop64

.tail16:
    mov     rax, LEN_N
    and     rax, 63
    shr     rax, 4
    jz      .hadd

.loop16:
    vmovups zmm16, [PTR_A]
    vfmadd231ps zmm0, zmm16, [PTR_B]
    add     PTR_A, 64
    add     PTR_B, 64
    dec     rax
    jnz     .loop16

.hadd:
    vaddps  zmm0, zmm0, zmm1
    vaddps  zmm2, zmm2, zmm3
    vaddps  zmm0, zmm0, zmm2
    vextractf64x4 ymm1, zmm0, 1
    vaddps  ymm0, ymm0, ymm1
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    mov     rax, LEN_N
    and     rax, 15
.scalar:
    test    rax, rax
    jz      .done_avx
    vmovss  xmm1, [PTR_A]
    vmulss  xmm1, xmm1, [PTR_B]
    vaddss  xmm0, xmm0, xmm1
    add     PTR_A, 4
    add     PTR_B, 4
    dec     rax
    jnz     .scalar

.done_avx:
    vzeroupper
.done:
    ret
