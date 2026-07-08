; float vecasm_sum_f32_avx512(const float* a, size_t n)
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_sum_f32_avx512

vecasm_sum_f32_avx512:
    vxorps  xmm0, xmm0, xmm0
    test    LEN_SUM, LEN_SUM
    jz      .done

    vxorps  zmm0, zmm0, zmm0
    vxorps  zmm1, zmm1, zmm1

    mov     rax, LEN_SUM
    shr     rax, 5                    ; 32 floats = 2 zmm
    jz      .tail16

.loop32:
    vaddps  zmm0, zmm0, [PTR_SUM]
    vaddps  zmm1, zmm1, [PTR_SUM + 64]
    add     PTR_SUM, 128
    dec     rax
    jnz     .loop32

.tail16:
    mov     rax, LEN_SUM
    and     rax, 31
    shr     rax, 4
    jz      .hadd
.loop16:
    vaddps  zmm0, zmm0, [PTR_SUM]
    add     PTR_SUM, 64
    dec     rax
    jnz     .loop16

.hadd:
    vaddps  zmm0, zmm0, zmm1
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
