; float vecasm_sum_f32_avx2(const float* a, size_t n)
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_sum_f32_avx2

vecasm_sum_f32_avx2:
    xorpd   xmm0, xmm0
    test    LEN_SUM, LEN_SUM
    jz      .done

    vxorps  ymm0, ymm0, ymm0
    vxorps  ymm1, ymm1, ymm1

    mov     rax, LEN_SUM
    shr     rax, 4
    jz      .tail8

.loop16:
    vaddps  ymm0, ymm0, [PTR_SUM]
    vaddps  ymm1, ymm1, [PTR_SUM + 32]
    add     PTR_SUM, 64
    dec     rax
    jnz     .loop16

.tail8:
    mov     rax, LEN_SUM
    and     rax, 15
    shr     rax, 3
    jz      .hadd
.loop8:
    vaddps  ymm0, ymm0, [PTR_SUM]
    add     PTR_SUM, 32
    dec     rax
    jnz     .loop8

.hadd:
    vaddps  ymm0, ymm0, ymm1
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    mov     rax, LEN_SUM
    and     rax, 7
    jz      .done_avx
.scalar:
    vaddss  xmm0, xmm0, [PTR_SUM]
    add     PTR_SUM, 4
    dec     rax
    jnz     .scalar

.done_avx:
    vzeroupper
.done:
    ret
