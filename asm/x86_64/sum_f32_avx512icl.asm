; float vecasm_sum_f32_avx512icl(const float* a, size_t n)
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_sum_f32_avx512icl

vecasm_sum_f32_avx512icl:
    vxorps  xmm0, xmm0, xmm0
    test    LEN_SUM, LEN_SUM
    jz      .done

    vxorps  zmm0, zmm0, zmm0
    vxorps  zmm1, zmm1, zmm1
    vxorps  zmm2, zmm2, zmm2
    vxorps  zmm3, zmm3, zmm3

    mov     rax, LEN_SUM
    shr     rax, 6
    jz      .tail16

.loop64:
    prefetchnta [PTR_SUM + 512]
    vaddps  zmm0, zmm0, [PTR_SUM]
    vaddps  zmm1, zmm1, [PTR_SUM + 64]
    vaddps  zmm2, zmm2, [PTR_SUM + 128]
    vaddps  zmm3, zmm3, [PTR_SUM + 192]
    add     PTR_SUM, 256
    dec     rax
    jnz     .loop64

.tail16:
    mov     rax, LEN_SUM
    and     rax, 63
    shr     rax, 4
    jz      .hadd
.loop16:
    vaddps  zmm0, zmm0, [PTR_SUM]
    add     PTR_SUM, 64
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

    mov     rax, LEN_SUM
    and     rax, 15
.scalar:
    test    rax, rax
    jz      .done_avx
    vaddss  xmm0, xmm0, [PTR_SUM]
    add     PTR_SUM, 4
    dec     rax
    jnz     .scalar

.done_avx:
    vzeroupper
.done:
    ret
