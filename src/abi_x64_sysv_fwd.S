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

# Copy function pointer to R11, in order to save it through argument forwarding.
# Save RSP in RBX (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    .cfi_def_cfa rsp, 8
    endbr64
    movq %rdi, %r11
    pushq %rbx
    .cfi_def_cfa rsp, 16
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
