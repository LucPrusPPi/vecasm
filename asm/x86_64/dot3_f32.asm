; float vecasm_dot3_f32_asm(const float a[3], const float b[3])
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_dot3_f32_asm

vecasm_dot3_f32_asm:
%ifdef VECASM_ABI_WIN64
    mov     rax, rcx
    mov     r8, rdx
%else
    mov     rax, rdi
    mov     r8, rsi
%endif
    vmovss  xmm0, [rax]
    vmulss  xmm0, xmm0, [r8]
    vmovss  xmm1, [rax + 4]
    vmulss  xmm1, xmm1, [r8 + 4]
    vaddss  xmm0, xmm0, xmm1
    vmovss  xmm1, [rax + 8]
    vmulss  xmm1, xmm1, [r8 + 8]
    vaddss  xmm0, xmm0, xmm1
    ret
