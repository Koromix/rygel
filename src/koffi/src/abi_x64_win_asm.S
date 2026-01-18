# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#define SYMBOL(Symbol) Symbol

# Forward
# ----------------------------

.global ForwardCallG
.global ForwardCallF
.global ForwardCallD
.global ForwardCallXG
.global ForwardCallXF
.global ForwardCallXD

# Copy function pointer to RAX, in order to save it through argument forwarding.
# Also make a copy of the SP to CallData::old_sp because the callback system might need it.
# Save RSP in RBX (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    endbr64
    movq %rcx, %rax
    push %rbp
    movq %rsp, %rbp
    movq %rsp, (%r8)
    movq %rdx, %rsp
.endm

# Call native function.
# Once done, restore normal stack pointer and return.
# The return value is passed untouched through RAX or XMM0.
.macro epilogue
    call *%rax
    movq %rbp, %rsp
    pop %rbp
    ret
.endm

# Prepare integer argument registers from array passed by caller.
.macro forward_gpr
    movq 24(%rdx), %r9
    movq 16(%rdx), %r8
    movq 0(%rdx), %rcx
    movq 8(%rdx), %rdx
.endm

# Prepare XMM argument registers from array passed by caller.
.macro forward_xmm
    movsd 24(%rdx), %xmm3
    movsd 16(%rdx), %xmm2
    movsd 8(%rdx), %xmm1
    movsd 0(%rdx), %xmm0
.endm

ForwardCallG:
    prologue
    forward_gpr
    epilogue

ForwardCallF:
    prologue
    forward_gpr
    epilogue

ForwardCallD:
    prologue
    forward_gpr
    epilogue

ForwardCallXG:
    prologue
    forward_xmm
    forward_gpr
    epilogue

ForwardCallXF:
    prologue
    forward_xmm
    forward_gpr
    epilogue

ForwardCallXD:
    prologue
    forward_xmm
    forward_gpr
    epilogue

# Callbacks
# ----------------------------

.global RelayCallback
.global CallSwitchStack

# First, make a copy of the GPR argument registers (rcx, rdx, r8, r9).
# Then call the C function RelayCallback with the following arguments:
# static trampoline ID, a pointer to the saved GPR array, a pointer to the stack
# arguments of this call, and a pointer to a struct that will contain the result registers.
# After the call, simply load these registers from the output struct.
.macro trampoline id
    endbr64
    subq $120, %rsp
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movq $\id, %rcx
    leaq 32(%rsp), %rdx
    leaq 160(%rsp), %r8
    leaq 96(%rsp), %r9
    call RelayCallback
    movq 96(%rsp), %rax
    addq $120, %rsp
    ret
.endm

# Same thing, but also forward the XMM argument registers and load the XMM result registers.
.macro trampoline_vec id
    endbr64
    subq $120, %rsp
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movsd %xmm0, 64(%rsp)
    movsd %xmm1, 72(%rsp)
    movsd %xmm2, 80(%rsp)
    movsd %xmm3, 88(%rsp)
    movq $\id, %rcx
    leaq 32(%rsp), %rdx
    leaq 160(%rsp), %r8
    leaq 96(%rsp), %r9
    call RelayCallback
    movq 96(%rsp), %rax
    movsd 104(%rsp), %xmm0
    addq $120, %rsp
    ret
.endm

# When a callback is relayed, Koffi will call into Node.js and V8 to execute Javascript.
# The problem is that we're still running on the separate Koffi stack, and V8 will
# probably misdetect this as a "stack overflow". We have to restore the old
# stack pointer, call Node.js/V8 and go back to ours.
# The first three parameters (rcx, rdx, r8) are passed through untouched.
CallSwitchStack:
    endbr64
    push %rbp
    movq %rsp, %rbp
    movq 56(%rsp), %rax
    movq %rsp, %r10
    movq 48(%rsp), %r11
    subq 0(%r11), %r10
    andq $-16, %r10
    movq %r10, 8(%r11)
    leaq -32(%r9), %rsp
    call *%rax
    movq %rbp, %rsp
    pop %rbp
    ret

# Trampolines
# ----------------------------

#include "trampolines/gnu.inc"
