# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see https://www.gnu.org/licenses/.

#ifdef __APPLE__
    #define SYMBOL(Symbol) _ ## Symbol
#else
    #define SYMBOL(Symbol) Symbol
#endif

# Forward
# ----------------------------

# These five are the same, but they differ (in the C side) by their return type.
# Unlike the five next functions, these ones don't forward XMM argument registers.
.global SYMBOL(ForwardCallGG)
.global SYMBOL(ForwardCallF)
.global SYMBOL(ForwardCallDG)
.global SYMBOL(ForwardCallGD)
.global SYMBOL(ForwardCallDD)

# The X variants are slightly slower, and are used when XMM arguments must be forwarded.
.global SYMBOL(ForwardCallXGG)
.global SYMBOL(ForwardCallXF)
.global SYMBOL(ForwardCallXDG)
.global SYMBOL(ForwardCallXGD)
.global SYMBOL(ForwardCallXDD)

#define ENDBR64 .byte 0xf3, 0x0f, 0x1e, 0xfa

# Copy function pointer to R11, in order to save it through argument forwarding.
# Also make a copy of the SP to CallData::old_sp because the callback system might need it.
# Save RSP in RBX (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    .cfi_def_cfa rsp, 8
    ENDBR64
    movq %rdi, %r11
    pushq %rbx
    .cfi_def_cfa rsp, 16
    movq %rsp, (%rdx)
    movq %rsp, %rbx
    .cfi_def_cfa rbx, 16
    leaq 112(%rsi), %rsp
.endm

# Call native function.
# Once done, restore normal stack pointer and return.
# The return value is passed untouched through RAX or XMM0.
.macro epilogue
    call *%r11
    movq %rbx, %rsp
    popq %rbx
    .cfi_def_cfa rsp, 8
    ret
    .cfi_endproc
.endm

# Prepare integer argument registers from array passed by caller.
.macro forward_int
    movq 40(%rsi), %r9
    movq 32(%rsi), %r8
    movq 24(%rsi), %rcx
    movq 16(%rsi), %rdx
    movq 0(%rsi), %rdi
    movq 8(%rsi), %rsi
.endm

# Prepare XMM argument registers from array passed by caller.
.macro forward_xmm
    movsd 104(%rsi), %xmm7
    movsd 96(%rsi), %xmm6
    movsd 88(%rsi), %xmm5
    movsd 80(%rsi), %xmm4
    movsd 72(%rsi), %xmm3
    movsd 64(%rsi), %xmm2
    movsd 56(%rsi), %xmm1
    movsd 48(%rsi), %xmm0
.endm

SYMBOL(ForwardCallGG):
    prologue
    forward_int
    movb $0, %al
    epilogue

SYMBOL(ForwardCallF):
    prologue
    forward_int
    movb $0, %al
    epilogue

SYMBOL(ForwardCallDG):
    prologue
    forward_int
    movb $0, %al
    epilogue

SYMBOL(ForwardCallGD):
    prologue
    forward_int
    movb $0, %al
    epilogue

SYMBOL(ForwardCallDD):
    prologue
    forward_int
    movb $0, %al
    epilogue

SYMBOL(ForwardCallXGG):
    prologue
    forward_xmm
    forward_int
    movb $8, %al
    epilogue

SYMBOL(ForwardCallXF):
    prologue
    forward_xmm
    forward_int
    movb $8, %al
    epilogue

SYMBOL(ForwardCallXDG):
    prologue
    forward_xmm
    forward_int
    movb $8, %al
    epilogue

SYMBOL(ForwardCallXGD):
    prologue
    forward_xmm
    forward_int
    movb $8, %al
    epilogue

SYMBOL(ForwardCallXDD):
    prologue
    forward_xmm
    forward_int
    movb $8, %al
    epilogue

# Callback trampolines
# ----------------------------

.global SYMBOL(Trampoline0)
.global SYMBOL(Trampoline1)
.global SYMBOL(Trampoline2)
.global SYMBOL(Trampoline3)
.global SYMBOL(Trampoline4)
.global SYMBOL(Trampoline5)
.global SYMBOL(Trampoline6)
.global SYMBOL(Trampoline7)
.global SYMBOL(Trampoline8)
.global SYMBOL(Trampoline9)
.global SYMBOL(Trampoline10)
.global SYMBOL(Trampoline11)
.global SYMBOL(Trampoline12)
.global SYMBOL(Trampoline13)
.global SYMBOL(Trampoline14)
.global SYMBOL(Trampoline15)
.global SYMBOL(TrampolineX0)
.global SYMBOL(TrampolineX1)
.global SYMBOL(TrampolineX2)
.global SYMBOL(TrampolineX3)
.global SYMBOL(TrampolineX4)
.global SYMBOL(TrampolineX5)
.global SYMBOL(TrampolineX6)
.global SYMBOL(TrampolineX7)
.global SYMBOL(TrampolineX8)
.global SYMBOL(TrampolineX9)
.global SYMBOL(TrampolineX10)
.global SYMBOL(TrampolineX11)
.global SYMBOL(TrampolineX12)
.global SYMBOL(TrampolineX13)
.global SYMBOL(TrampolineX14)
.global SYMBOL(TrampolineX15)
.global SYMBOL(RelayCallback)
.global SYMBOL(CallSwitchStack)

# First, make a copy of the GPR argument registers (rdi, rsi, rdx, rcx, r8, r9).
# Then call the C function RelayCallback with the following arguments:
# static trampoline ID, a pointer to the saved GPR array, a pointer to the stack
# arguments of this call, and a pointer to a struct that will contain the result registers.
# After the call, simply load these registers from the output struct.
.macro trampoline id
    .cfi_startproc
    .cfi_def_cfa rsp, 8
    ENDBR64
    subq $152, %rsp
    .cfi_def_cfa rsp, 160
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    movq %r8, 32(%rsp)
    movq %r9, 40(%rsp)
    movq $\id, %rdi
    movq %rsp, %rsi
    leaq 160(%rsp), %rdx
    leaq 112(%rsp), %rcx
    call SYMBOL(RelayCallback)
    movq 112(%rsp), %rax
    movq 120(%rsp), %rdx
    addq $152, %rsp
    .cfi_def_cfa rsp, 8
    ret
    .cfi_endproc
.endm

# Same thing, but also forward the XMM argument registers and load the XMM result registers.
.macro trampoline_xmm id
    .cfi_startproc
    .cfi_def_cfa rsp, 8
    ENDBR64
    subq $152, %rsp
    .cfi_def_cfa rsp, 160
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    movq %r8, 32(%rsp)
    movq %r9, 40(%rsp)
    movsd %xmm0, 48(%rsp)
    movsd %xmm1, 56(%rsp)
    movsd %xmm2, 64(%rsp)
    movsd %xmm3, 72(%rsp)
    movsd %xmm4, 80(%rsp)
    movsd %xmm5, 88(%rsp)
    movsd %xmm6, 96(%rsp)
    movsd %xmm7, 104(%rsp)
    movq $\id, %rdi
    movq %rsp, %rsi
    leaq 160(%rsp), %rdx
    leaq 112(%rsp), %rcx
    call SYMBOL(RelayCallback)
    movq 112(%rsp), %rax
    movq 120(%rsp), %rdx
    movsd 128(%rsp), %xmm0
    movsd 136(%rsp), %xmm1
    addq $152, %rsp
    .cfi_def_cfa rsp, 8
    ret
    .cfi_endproc
.endm

SYMBOL(Trampoline0):
    trampoline 0
SYMBOL(Trampoline1):
    trampoline 1
SYMBOL(Trampoline2):
    trampoline 2
SYMBOL(Trampoline3):
    trampoline 3
SYMBOL(Trampoline4):
    trampoline 4
SYMBOL(Trampoline5):
    trampoline 5
SYMBOL(Trampoline6):
    trampoline 6
SYMBOL(Trampoline7):
    trampoline 7
SYMBOL(Trampoline8):
    trampoline 8
SYMBOL(Trampoline9):
    trampoline 9
SYMBOL(Trampoline10):
    trampoline 10
SYMBOL(Trampoline11):
    trampoline 11
SYMBOL(Trampoline12):
    trampoline 12
SYMBOL(Trampoline13):
    trampoline 13
SYMBOL(Trampoline14):
    trampoline 14
SYMBOL(Trampoline15):
    trampoline 15

SYMBOL(TrampolineX0):
    trampoline_xmm 0
SYMBOL(TrampolineX1):
    trampoline_xmm 1
SYMBOL(TrampolineX2):
    trampoline_xmm 2
SYMBOL(TrampolineX3):
    trampoline_xmm 3
SYMBOL(TrampolineX4):
    trampoline_xmm 4
SYMBOL(TrampolineX5):
    trampoline_xmm 5
SYMBOL(TrampolineX6):
    trampoline_xmm 6
SYMBOL(TrampolineX7):
    trampoline_xmm 7
SYMBOL(TrampolineX8):
    trampoline_xmm 8
SYMBOL(TrampolineX9):
    trampoline_xmm 9
SYMBOL(TrampolineX10):
    trampoline_xmm 10
SYMBOL(TrampolineX11):
    trampoline_xmm 11
SYMBOL(TrampolineX12):
    trampoline_xmm 12
SYMBOL(TrampolineX13):
    trampoline_xmm 13
SYMBOL(TrampolineX14):
    trampoline_xmm 14
SYMBOL(TrampolineX15):
    trampoline_xmm 15

# When a callback is relayed, Koffi will call into Node.js and V8 to execute Javascript.
# The problem is that we're still running on the separate Koffi stack, and V8 will
# probably misdetect this as a "stack overflow". We have to restore the old
# stack pointer, call Node.js/V8 and go back to ours.
# The first three parameters (rdi, rsi, rdx) are passed through untouched.
SYMBOL(CallSwitchStack):
    .cfi_startproc
    .cfi_def_cfa rsp, 8
    ENDBR64
    push %rbx
    .cfi_def_cfa rsp, 16
    movq %rsp, %rbx
    movq %rsp, %r10
    subq 0(%r8), %r10
    andq $-16, %r10
    movq %r10, 8(%r8)
    movq %rcx, %rsp
    .cfi_def_cfa rsp, 16
    call *%r9
    mov %rbx, %rsp
    .cfi_def_cfa rsp, 16
    pop %rbx
    .cfi_def_cfa rsp, 8
    ret
    .cfi_endproc
