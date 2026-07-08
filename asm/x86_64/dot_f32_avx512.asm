; float vecasm_dot_f32_avx512(const float* a, const float* b, size_t n)
; Generic AVX-512F: ZMM, mul+add. Uses zmm0-5, zmm16-19 only (Win64-safe).
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_dot_f32_avx512

vecasm_dot_f32_avx512:
    vxorps  xmm0, xmm0, xmm0
    test    LEN_N, LEN_N
    jz      .done

    vxorps  zmm0, zmm0, zmm0
    vxorps  zmm1, zmm1, zmm1
    vxorps  zmm2, zmm2, zmm2
    vxorps  zmm3, zmm3, zmm3

    mov     rax, LEN_N
    shr     rax, 6                    ; 64 floats = 4x16
    jz      .tail16

.loop64:
    vmovups zmm16, [PTR_A]
    vmovups zmm17, [PTR_B]
    vmulps  zmm16, zmm16, zmm17
    vaddps  zmm0, zmm0, zmm16

    vmovups zmm16, [PTR_A + 64]
    vmovups zmm17, [PTR_B + 64]
    vmulps  zmm16, zmm16, zmm17
    vaddps  zmm1, zmm1, zmm16

    vmovups zmm16, [PTR_A + 128]
    vmovups zmm17, [PTR_B + 128]
    vmulps  zmm16, zmm16, zmm17
    vaddps  zmm2, zmm2, zmm16

    vmovups zmm16, [PTR_A + 192]
    vmovups zmm17, [PTR_B + 192]
    vmulps  zmm16, zmm16, zmm17
    vaddps  zmm3, zmm3, zmm16

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
    vmovups zmm17, [PTR_B]
    vmulps  zmm16, zmm16, zmm17
    vaddps  zmm0, zmm0, zmm16
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
