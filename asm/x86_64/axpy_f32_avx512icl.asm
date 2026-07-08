; void vecasm_axpy_f32_avx512icl(float alpha, const float* x, float* y, size_t n)
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_axpy_f32_avx512icl

vecasm_axpy_f32_avx512icl:
    test    LEN_AX, LEN_AX
    jz      .done

    vbroadcastss zmm0, xmm0
    mov     rax, LEN_AX
    shr     rax, 5                    ; 32 floats = 2 zmm
    jz      .tail16

.loop32:
    prefetchnta [PTR_X + 256]
    prefetchnta [PTR_Y + 256]
    vmovups zmm1, [PTR_X]
    vfmadd213ps zmm1, zmm0, [PTR_Y]   ; alpha*x + y
    vmovups [PTR_Y], zmm1
    vmovups zmm1, [PTR_X + 64]
    vfmadd213ps zmm1, zmm0, [PTR_Y + 64]
    vmovups [PTR_Y + 64], zmm1
    add     PTR_X, 128
    add     PTR_Y, 128
    dec     rax
    jnz     .loop32

.tail16:
    mov     rax, LEN_AX
    and     rax, 31
    shr     rax, 4
    jz      .tail1
.loop16:
    vmovups zmm1, [PTR_X]
    vfmadd213ps zmm1, zmm0, [PTR_Y]
    vmovups [PTR_Y], zmm1
    add     PTR_X, 64
    add     PTR_Y, 64
    dec     rax
    jnz     .loop16

.tail1:
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
