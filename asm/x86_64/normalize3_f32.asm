; void vecasm_normalize3_f32_asm(const float in[3], float out[3])
%include "abi.inc"

default rel
bits 64

section .text
global vecasm_normalize3_f32_asm

vecasm_normalize3_f32_asm:
%ifdef VECASM_ABI_WIN64
    mov     rax, rcx
    mov     r8, rdx
%else
    mov     rax, rdi
    mov     r8, rsi
%endif
    vmovss  xmm0, [rax]
    vmovss  xmm1, [rax + 4]
    vmovss  xmm2, [rax + 8]
    vmulss  xmm3, xmm0, xmm0
    vmulss  xmm4, xmm1, xmm1
    vaddss  xmm3, xmm3, xmm4
    vmulss  xmm4, xmm2, xmm2
    vaddss  xmm3, xmm3, xmm4

    vxorps  xmm5, xmm5, xmm5
    vucomiss xmm3, xmm5
    jbe     .zero

    vsqrtss xmm3, xmm3, xmm3
    vdivss  xmm0, xmm0, xmm3
    vdivss  xmm1, xmm1, xmm3
    vdivss  xmm2, xmm2, xmm3
    vmovss  [r8], xmm0
    vmovss  [r8 + 4], xmm1
    vmovss  [r8 + 8], xmm2
    ret

.zero:
    vmovss  [r8], xmm5
    vmovss  [r8 + 4], xmm5
    vmovss  [r8 + 8], xmm5
    ret
