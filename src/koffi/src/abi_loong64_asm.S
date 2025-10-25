# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#define SYMBOL(Symbol) Symbol

# Forward
# ----------------------------

# These three are the same, but they differ (in the C side) by their return type.
# Unlike the three next functions, these ones don't forward FA argument registers.
.global ForwardCallGG
.global ForwardCallF
.global ForwardCallDG
.global ForwardCallGD
.global ForwardCallDD

# The X variants are slightly slower, and are used when FA arguments must be forwarded.
.global ForwardCallXGG
.global ForwardCallXF
.global ForwardCallXDG
.global ForwardCallXGD
.global ForwardCallXDD

# Copy function pointer to t7, in order to save it through argument forwarding.
# Also make a copy of the SP to CallData::old_sp because the callback system might need it.
# Save SP in FP, and use carefully assembled stack provided by caller.
.macro prologue
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 0
    st.d $fp, $sp, 8
    move $fp, $sp
    st.d $fp, $a2, 0
    move $t7, $a0
    addi.d $sp, $a1, 128
.endm

# Call native function.
# Once done, restore normal stack pointer and return.
# The return value is passed untouched through registers.
.macro epilogue
    jirl $ra, $t7,0
    move $sp, $fp
    ld.d $ra, $sp, 0
    ld.d $fp, $sp, 8
    addi.d $sp, $sp, 16
    jr $ra
.endm

# Prepare general purpose argument registers from array passed by caller.
.macro forward_gpr
    ld.d $a7, $a1, 120
    ld.d $a6, $a1, 112
    ld.d $a5, $a1, 104
    ld.d $a4, $a1, 96
    ld.d $a3, $a1, 88
    ld.d $a2, $a1, 80
    ld.d $a0, $a1, 64
    ld.d $a1, $a1, 72
.endm

# Prepare vector argument registers from array passed by caller.
.macro forward_vec
    fld.d $fa7, $a1, 56
    fld.d $fa6, $a1, 48
    fld.d $fa5, $a1, 40
    fld.d $fa4, $a1, 32
    fld.d $fa3, $a1, 24
    fld.d $fa2, $a1, 16
    fld.d $fa1, $a1, 8
    fld.d $fa0, $a1, 0
.endm

ForwardCallGG:
    prologue
    forward_gpr
    epilogue

ForwardCallF:
    prologue
    forward_gpr
    epilogue

ForwardCallDG:
    prologue
    forward_gpr
    epilogue

ForwardCallGD:
    prologue
    forward_gpr
    epilogue

ForwardCallDD:
    prologue
    forward_gpr
    epilogue

ForwardCallXGG:
    prologue
    forward_vec
    forward_gpr
    epilogue

ForwardCallXF:
    prologue
    forward_vec
    forward_gpr
    epilogue

ForwardCallXDG:
    prologue
    forward_vec
    forward_gpr
    epilogue

ForwardCallXGD:
    prologue
    forward_vec
    forward_gpr
    epilogue

ForwardCallXDD:
    prologue
    forward_vec
    forward_gpr
    epilogue

# Callbacks
# ----------------------------

.global RelayCallback
.global CallSwitchStack

# First, make a copy of the GPR argument registers (a0 to a7).
# Then call the C function RelayCallback with the following arguments:
# static trampoline ID, a pointer to the saved GPR array, a pointer to the stack
# arguments of this call, and a pointer to a struct that will contain the result registers.
# After the call, simply load these registers from the output struct.
.macro trampoline id
    addi.d $sp, $sp, -176
    st.d $ra, $sp, 0
    st.d $a0, $sp, 8
    st.d $a1, $sp, 16
    st.d $a2, $sp, 24
    st.d $a3, $sp, 32
    st.d $a4, $sp, 40
    st.d $a5, $sp, 48
    st.d $a6, $sp, 56
    st.d $a7, $sp, 64
    li.d $a0, \id
    addi.d $a1, $sp, 8
    addi.d $a2, $sp, 176
    addi.d $a3, $sp, 136
    bl RelayCallback
    ld.d $ra, $sp, 0
    ld.d $a0, $sp, 136
    ld.d $a1, $sp, 144
    addi.d $sp, $sp, 176
    jr $ra
.endm

# Same thing, but also forwards the floating-point argument registers and loads them at the end.
.macro trampoline_vec id
    addi.d $sp, $sp, -176
    st.d $ra, $sp, 0
    st.d $a0, $sp, 8
    st.d $a1, $sp, 16
    st.d $a2, $sp, 24
    st.d $a3, $sp, 32
    st.d $a4, $sp, 40
    st.d $a5, $sp, 48
    st.d $a6, $sp, 56
    st.d $a7, $sp, 64
    fst.d $fa0, $sp, 72
    fst.d $fa1, $sp, 80
    fst.d $fa2, $sp, 88
    fst.d $fa3, $sp, 96
    fst.d $fa4, $sp, 104
    fst.d $fa5, $sp, 112
    fst.d $fa6, $sp, 120
    fst.d $fa7, $sp, 128
    li.d $a0, \id
    addi.d $a1, $sp, 8
    addi.d $a2, $sp, 176
    addi.d $a3, $sp, 136
    bl RelayCallback
    ld.d $ra, $sp, 0
    ld.d $a0, $sp, 136
    ld.d $a1, $sp, 144
    fld.d $fa0, $sp, 152
    fld.d $fa1, $sp, 160
    addi.d $sp, $sp, 176
    jr $ra
.endm

# When a callback is relayed, Koffi will call into Node.js and V8 to execute Javascript.
# The problem is that we're still running on the separate Koffi stack, and V8 will
# probably misdetect this as a "stack overflow". We have to restore the old
# stack pointer, call Node.js/V8 and go back to ours.
# The first three parameters (a0, a1, a2) are passed through untouched.
CallSwitchStack:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 0
    st.d $fp, $sp, 8
    move $fp, $sp
    ld.d $t0, $a4, 0
    sub.d $t0, $sp, $t0
    li.d $t1, -16
    and $t0, $t0, $t1
    st.d $t0, $a4, 8
    move $sp, $a3
    jirl $ra, $a5, 0
    move $sp, $fp
    ld.d $ra, $sp, 0
    ld.d $fp, $sp, 8
    addi.d $sp, $sp, 16
    jr $ra

# Trampolines
# ----------------------------

#include "trampolines/gnu.inc"
