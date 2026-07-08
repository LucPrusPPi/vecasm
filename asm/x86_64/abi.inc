; Shared ABI macros for vecasm x86_64 NASM kernels.
; Define one of: VECASM_ABI_WIN64 or VECASM_ABI_SYSV (via -D on nasm cmdline).

%ifndef VECASM_ABI_INC
%define VECASM_ABI_INC

%ifdef VECASM_ABI_WIN64
  ; Microsoft x64: RCX, RDX, R8, R9 / XMM0..3
  %define PTR_A   rcx
  %define PTR_B   rdx
  %define LEN_N   r8
  %define PTR_X   rdx
  %define PTR_Y   r8
  %define LEN_AX  r9
  %define PTR_SUM rcx
  %define LEN_SUM rdx
%elifdef VECASM_ABI_SYSV
  ; System V AMD64 (Linux/macOS): RDI, RSI, RDX, RCX / XMM0..7
  %define PTR_A   rdi
  %define PTR_B   rsi
  %define LEN_N   rdx
  %define PTR_X   rdi
  %define PTR_Y   rsi
  %define LEN_AX  rdx
  %define PTR_SUM rdi
  %define LEN_SUM rsi
%else
  %error "Define VECASM_ABI_WIN64 or VECASM_ABI_SYSV"
%endif

%endif ; VECASM_ABI_INC
