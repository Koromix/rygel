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

#ifdef __APPLE__
# Fix "arm64 function not 4-byte aligned" linker warnings
.p2align 2
#endif

# These three are the same, but they differ (in the C side) by their return type.
# Unlike the three next functions, these ones don't forward XMM argument registers.
.global SYMBOL(ForwardCallGG)
.global SYMBOL(ForwardCallF)
.global SYMBOL(ForwardCallDDDD)

# The X variants are slightly slower, and are used when XMM arguments must be forwarded.
.global SYMBOL(ForwardCallXGG)
.global SYMBOL(ForwardCallXF)
.global SYMBOL(ForwardCallXDDDD)

# Copy function pointer to r9, in order to save it through argument forwarding.
# Also make a copy of the SP to CallData::old_sp because the callback system might need it.
# Save RSP in r29 (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    stp x29, x30, [sp, -16]!
    .cfi_def_cfa sp, 16
    .cfi_offset 29, 16
    .cfi_offset 30, 8
    mov x29, sp
    .cfi_def_cfa x29, 16
    str x29, [x2, 0]
    mov x9, x0
    add sp, x1, #136
.endm

# Call native function.
# Once done, restore normal stack pointer and return.
# The return value is passed untouched through r0, r1, v0 and/or v1.
.macro epilogue
    blr x9
    mov sp, x29
    .cfi_def_cfa sp, 16
    ldp x29, x30, [sp], 16
    .cfi_def_cfa sp, 0
    .cfi_restore 30
    .cfi_restore 29
    ret
    .cfi_endproc
.endm

# Prepare general purpose argument registers from array passed by caller.
.macro forward_gpr
    ldr x8, [x1, 64]
    ldp x6, x7, [x1, 48]
    ldp x4, x5, [x1, 32]
    ldp x2, x3, [x1, 16]
    ldp x0, x1, [x1, 0]
.endm

# Prepare vector argument registers from array passed by caller.
.macro forward_vec
    ldp d6, d7, [x1, 120]
    ldp d4, d5, [x1, 104]
    ldp d2, d3, [x1, 88]
    ldp d0, d1, [x1, 72]
.endm

SYMBOL(ForwardCallGG):
    prologue
    forward_gpr
    epilogue

SYMBOL(ForwardCallF):
    prologue
    forward_gpr
    epilogue

SYMBOL(ForwardCallDDDD):
    prologue
    forward_gpr
    epilogue

SYMBOL(ForwardCallXGG):
    prologue
    forward_vec
    forward_gpr
    epilogue

SYMBOL(ForwardCallXF):
    prologue
    forward_vec
    forward_gpr
    epilogue

SYMBOL(ForwardCallXDDDD):
    prologue
    forward_vec
    forward_gpr
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

# First, make a copy of the GPR argument registers (x0 to x7).
# Then call the C function RelayCallback with the following arguments:
# static trampoline ID, a pointer to the saved GPR array, a pointer to the stack
# arguments of this call, and a pointer to a struct that will contain the result registers.
# After the call, simply load these registers from the output struct.
.macro trampoline id
    .cfi_startproc
    stp x29, x30, [sp, -16]!
    .cfi_def_cfa sp, 16
    .cfi_offset 29, 16
    .cfi_offset 30, 8
    sub sp, sp, #192
    .cfi_def_cfa sp, 208
    stp x0, x1, [sp, 0]
    stp x2, x3, [sp, 16]
    stp x4, x5, [sp, 32]
    stp x6, x7, [sp, 48]
    str x8, [sp, 64]
    mov x0, \id
    mov x1, sp
    add x2, sp, #208
    add x3, sp, #136
    bl SYMBOL(RelayCallback)
    ldp x0, x1, [sp, 136]
    add sp, sp, #192
    .cfi_def_cfa sp, 16
    ldp x29, x30, [sp], 16
    .cfi_def_cfa sp, 0
    .cfi_restore 30
    .cfi_restore 29
    ret
    .cfi_endproc
.endm

# Same thing, but also forwards the floating-point argument registers and loads them at the end.
.macro trampoline_vec id
    .cfi_startproc
    stp x29, x30, [sp, -16]!
    .cfi_def_cfa sp, 16
    .cfi_offset 29, 16
    .cfi_offset 30, 8
    sub sp, sp, #192
    .cfi_def_cfa sp, 208
    stp x0, x1, [sp, 0]
    stp x2, x3, [sp, 16]
    stp x4, x5, [sp, 32]
    stp x6, x7, [sp, 48]
    str x8, [sp, 64]
    stp d0, d1, [sp, 72]
    stp d2, d3, [sp, 88]
    stp d4, d5, [sp, 104]
    stp d6, d7, [sp, 120]
    mov x0, \id
    mov x1, sp
    add x2, sp, #208
    add x3, sp, #136
    bl SYMBOL(RelayCallback)
    ldp x0, x1, [sp, 136]
    ldp d0, d1, [sp, 152]
    ldp d2, d3, [sp, 168]
    add sp, sp, #192
    .cfi_def_cfa sp, 16
    ldp x29, x30, [sp], 16
    .cfi_def_cfa sp, 0
    .cfi_restore 30
    .cfi_restore 29
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
    trampoline_vec 0
SYMBOL(TrampolineX1):
    trampoline_vec 1
SYMBOL(TrampolineX2):
    trampoline_vec 2
SYMBOL(TrampolineX3):
    trampoline_vec 3
SYMBOL(TrampolineX4):
    trampoline_vec 4
SYMBOL(TrampolineX5):
    trampoline_vec 5
SYMBOL(TrampolineX6):
    trampoline_vec 6
SYMBOL(TrampolineX7):
    trampoline_vec 7
SYMBOL(TrampolineX8):
    trampoline_vec 8
SYMBOL(TrampolineX9):
    trampoline_vec 9
SYMBOL(TrampolineX10):
    trampoline_vec 10
SYMBOL(TrampolineX11):
    trampoline_vec 11
SYMBOL(TrampolineX12):
    trampoline_vec 12
SYMBOL(TrampolineX13):
    trampoline_vec 13
SYMBOL(TrampolineX14):
    trampoline_vec 14
SYMBOL(TrampolineX15):
    trampoline_vec 15

# When a callback is relayed, Koffi will call into Node.js and V8 to execute Javascript.
# The problem is that we're still running on the separate Koffi stack, and V8 will
# probably misdetect this as a "stack overflow". We have to restore the old
# stack pointer, call Node.js/V8 and go back to ours.
# The first three parameters (x0, x1, x2) are passed through untouched.
SYMBOL(CallSwitchStack):
    .cfi_startproc
    stp x29, x30, [sp, -16]!
    .cfi_def_cfa sp, 16
    .cfi_offset 29, 16
    .cfi_offset 30, 8
    mov x29, sp
    ldr x9, [x4, 0]
    sub x9, sp, x9
    and x9, x9, #-16
    str x9, [x4, 8]
    mov sp, x3
    blr x5
    mov sp, x29
    .cfi_def_cfa sp, 16
    ldp x29, x30, [sp], 16
    .cfi_def_cfa sp, 0
    .cfi_restore 30
    .cfi_restore 29
    ret
    .cfi_endproc
