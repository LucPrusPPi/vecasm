; void vecasm_axpy_f32_avx2(float alpha, const float* x, float* y, size_t n)
; alpha always in XMM0 (Win64 and SysV).
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_axpy_f32_avx2

vecasm_axpy_f32_avx2:
    test    LEN_AX, LEN_AX
    jz      .done

    vbroadcastss ymm0, xmm0
    mov     rax, LEN_AX
    shr     rax, 3
    jz      .tail

.loop8:
    vmovups ymm1, [PTR_X]
    vmulps  ymm1, ymm1, ymm0
    vaddps  ymm1, ymm1, [PTR_Y]
    vmovups [PTR_Y], ymm1
    add     PTR_X, 32
    add     PTR_Y, 32
    dec     rax
    jnz     .loop8

.tail:
    mov     rax, LEN_AX
    and     rax, 7
    jz      .done_avx
.scalar:
    vmulss  xmm1, xmm0, [PTR_X]
    vaddss  xmm1, xmm1, [PTR_Y]
    vmovss  [PTR_Y], xmm1
    add     PTR_X, 4
    add     PTR_Y, 4
    dec     rax
    jnz     .scalar

.done_avx:
    vzeroupper
.done:
    ret
