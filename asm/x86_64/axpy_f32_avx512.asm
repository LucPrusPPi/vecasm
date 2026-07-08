; void vecasm_axpy_f32_avx512(float alpha, const float* x, float* y, size_t n)
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_axpy_f32_avx512

vecasm_axpy_f32_avx512:
    test    LEN_AX, LEN_AX
    jz      .done

    vbroadcastss zmm0, xmm0
    mov     rax, LEN_AX
    shr     rax, 4
    jz      .tail

.loop16:
    vmovups zmm1, [PTR_X]
    vmulps  zmm1, zmm1, zmm0
    vaddps  zmm1, zmm1, [PTR_Y]
    vmovups [PTR_Y], zmm1
    add     PTR_X, 64
    add     PTR_Y, 64
    dec     rax
    jnz     .loop16

.tail:
    mov     rax, LEN_AX
    and     rax, 15
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
