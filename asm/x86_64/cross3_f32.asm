; void vecasm_cross3_f32_asm(const float a[3], const float b[3], float out[3])
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_cross3_f32_asm

vecasm_cross3_f32_asm:
%ifdef VECASM_ABI_WIN64
    mov     rax, rcx
    mov     r9, rdx
    mov     r10, r8
%else
    mov     rax, rdi
    mov     r9, rsi
    mov     r10, rdx
%endif
    vmovss  xmm0, [rax + 4]
    vmulss  xmm0, xmm0, [r9 + 8]
    vmovss  xmm1, [rax + 8]
    vmulss  xmm1, xmm1, [r9 + 4]
    vsubss  xmm0, xmm0, xmm1
    vmovss  [r10], xmm0

    vmovss  xmm0, [rax + 8]
    vmulss  xmm0, xmm0, [r9]
    vmovss  xmm1, [rax]
    vmulss  xmm1, xmm1, [r9 + 8]
    vsubss  xmm0, xmm0, xmm1
    vmovss  [r10 + 4], xmm0

    vmovss  xmm0, [rax]
    vmulss  xmm0, xmm0, [r9 + 4]
    vmovss  xmm1, [rax + 4]
    vmulss  xmm1, xmm1, [r9]
    vsubss  xmm0, xmm0, xmm1
    vmovss  [r10 + 8], xmm0
    ret
