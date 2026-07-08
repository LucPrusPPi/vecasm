; float vecasm_dot_f32_avx2(const float* a, const float* b, size_t n)
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_dot_f32_avx2

vecasm_dot_f32_avx2:
    xorpd   xmm0, xmm0
    test    LEN_N, LEN_N
    jz      .done

    vxorps  ymm0, ymm0, ymm0
    vxorps  ymm1, ymm1, ymm1
    vxorps  ymm2, ymm2, ymm2
    vxorps  ymm3, ymm3, ymm3

    mov     rax, LEN_N
    shr     rax, 5
    jz      .tail16

.loop32:
    vmovups ymm4, [PTR_A]
    vmovups ymm5, [PTR_B]
    vmulps  ymm4, ymm4, ymm5
    vaddps  ymm0, ymm0, ymm4

    vmovups ymm4, [PTR_A + 32]
    vmovups ymm5, [PTR_B + 32]
    vmulps  ymm4, ymm4, ymm5
    vaddps  ymm1, ymm1, ymm4

    vmovups ymm4, [PTR_A + 64]
    vmovups ymm5, [PTR_B + 64]
    vmulps  ymm4, ymm4, ymm5
    vaddps  ymm2, ymm2, ymm4

    vmovups ymm4, [PTR_A + 96]
    vmovups ymm5, [PTR_B + 96]
    vmulps  ymm4, ymm4, ymm5
    vaddps  ymm3, ymm3, ymm4

    add     PTR_A, 128
    add     PTR_B, 128
    dec     rax
    jnz     .loop32

.tail16:
    mov     rax, LEN_N
    and     rax, 31
    shr     rax, 3
    jz      .tail1

.loop8:
    vmovups ymm4, [PTR_A]
    vmovups ymm5, [PTR_B]
    vmulps  ymm4, ymm4, ymm5
    vaddps  ymm0, ymm0, ymm4
    add     PTR_A, 32
    add     PTR_B, 32
    dec     rax
    jnz     .loop8

.tail1:
    vaddps  ymm0, ymm0, ymm1
    vaddps  ymm2, ymm2, ymm3
    vaddps  ymm0, ymm0, ymm2
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    mov     rax, LEN_N
    and     rax, 7
    jz      .done_avx
.scalar:
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
