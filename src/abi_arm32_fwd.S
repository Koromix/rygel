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

.syntax unified

# These three are the same, but they differ (in the C side) by their return type.
# Unlike the three next functions, these ones don't forward XMM argument registers.
.global ForwardCallGG
.global ForwardCallF
.global ForwardCallDDDD

# The X variants are slightly slower, and are used when XMM arguments must be forwarded.
.global ForwardCallXGG
.global ForwardCallXF
.global ForwardCallXDDDD

# Copy function pointer to r12, in order to save it through argument forwarding.
# Also make a copy of the SP to CallData::old_sp because the callback system might need it.
# Save RSP in fp (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    push {fp, lr}
    .cfi_def_cfa sp, 8
    .cfi_offset 11, 4
    .cfi_offset 14, 8
    mov fp, sp
    .cfi_def_cfa fp, 8
    str fp, [r2, 0]
    mov r12, r0
    mov sp, r1
    add sp, sp, #80
.endm

# Call native function.
# Once done, restore normal stack pointer and return.
# The return value is passed untouched through r0, r1, and or FP registers
.macro epilogue
    blx r12
    mov sp, fp
    .cfi_def_cfa sp, 8
    pop {fp, lr}
    .cfi_def_cfa sp, 0
    .cfi_restore 11
    .cfi_restore 14
    bx lr
    .cfi_endproc
.endm

# Prepare general purpose argument registers from array passed by caller.
.macro forward_int
    ldr r3, [r1, 76]
    ldr r2, [r1, 72]
    ldr r0, [r1, 64]
    ldr r1, [r1, 68]
.endm

# Prepare vector argument registers from array passed by caller.
.macro forward_vec
    vldr d7, [r1, 56]
    vldr d6, [r1, 48]
    vldr d5, [r1, 40]
    vldr d4, [r1, 32]
    vldr d3, [r1, 24]
    vldr d2, [r1, 16]
    vldr d1, [r1, 8]
    vldr d0, [r1, 0]
.endm

ForwardCallGG:
    prologue
    forward_int
    epilogue

ForwardCallF:
    prologue
    forward_int
    epilogue

ForwardCallDDDD:
    prologue
    forward_int
    epilogue

ForwardCallXGG:
    prologue
    forward_vec
    forward_int
    epilogue

ForwardCallXF:
    prologue
    forward_vec
    forward_int
    epilogue

ForwardCallXDDDD:
    prologue
    forward_vec
    forward_int
    epilogue

# Callback trampolines
# ----------------------------

.global Trampoline0
.global Trampoline1
.global Trampoline2
.global Trampoline3
.global Trampoline4
.global Trampoline5
.global Trampoline6
.global Trampoline7
.global Trampoline8
.global Trampoline9
.global Trampoline10
.global Trampoline11
.global Trampoline12
.global Trampoline13
.global Trampoline14
.global Trampoline15
.global TrampolineX0
.global TrampolineX1
.global TrampolineX2
.global TrampolineX3
.global TrampolineX4
.global TrampolineX5
.global TrampolineX6
.global TrampolineX7
.global TrampolineX8
.global TrampolineX9
.global TrampolineX10
.global TrampolineX11
.global TrampolineX12
.global TrampolineX13
.global TrampolineX14
.global TrampolineX15
.global RelayCallback
.global CallSwitchStack

# First, make a copy of the GPR argument registers (r0 to r7).
# Then call the C function RelayCallback with the following arguments:
# static trampoline ID, a pointer to the saved GPR array, a pointer to the stack
# arguments of this call, and a pointer to a struct that will contain the result registers.
# After the call, simply load these registers from the output struct.
.macro trampoline id
    .cfi_startproc
    push {fp, lr}
    .cfi_def_cfa sp, 8
    .cfi_offset 11, 4
    .cfi_offset 14, 8
    sub sp, sp, #120
    .cfi_def_cfa sp, 128
    str r0, [sp, 64]
    str r1, [sp, 68]
    str r2, [sp, 72]
    str r3, [sp, 76]
    mov r0, \id
    mov r1, sp
    add r2, sp, #128
    add r3, sp, #80
    bl RelayCallback
    ldr r0, [sp, 80]
    ldr r1, [sp, 84]
    add sp, sp, #120
    .cfi_def_cfa sp, 8
    pop {fp, lr}
    .cfi_def_cfa sp, 0
    .cfi_restore 11
    .cfi_restore 14
    bx lr
    .cfi_endproc
.endm

# Same thing, but also forwards the floating-point argument registers and loads them at the end.
.macro trampoline_vec id
    .cfi_startproc
    push {fp, lr}
    .cfi_def_cfa sp, 8
    .cfi_offset 11, 4
    .cfi_offset 14, 8
    sub sp, sp, #120
    .cfi_def_cfa sp, 128
    str r0, [sp, 64]
    str r1, [sp, 68]
    str r2, [sp, 72]
    str r3, [sp, 76]
    vstr d0, [sp, 0]
    vstr d1, [sp, 8]
    vstr d2, [sp, 16]
    vstr d3, [sp, 24]
    vstr d4, [sp, 32]
    vstr d5, [sp, 40]
    vstr d6, [sp, 48]
    vstr d7, [sp, 56]
    mov r0, \id
    mov r1, sp
    add r2, sp, #128
    add r3, sp, #80
    bl RelayCallback
    ldr r0, [sp, 80]
    ldr r1, [sp, 84]
    vldr d0, [sp, 88]
    vldr d1, [sp, 96]
    vldr d2, [sp, 104]
    vldr d3, [sp, 112]
    add sp, sp, #120
    .cfi_def_cfa sp, 8
    pop {fp, lr}
    .cfi_def_cfa sp, 0
    .cfi_restore 11
    .cfi_restore 14
    bx lr
    .cfi_endproc
.endm

Trampoline0:
    trampoline 0
Trampoline1:
    trampoline 1
Trampoline2:
    trampoline 2
Trampoline3:
    trampoline 3
Trampoline4:
    trampoline 4
Trampoline5:
    trampoline 5
Trampoline6:
    trampoline 6
Trampoline7:
    trampoline 7
Trampoline8:
    trampoline 8
Trampoline9:
    trampoline 9
Trampoline10:
    trampoline 10
Trampoline11:
    trampoline 11
Trampoline12:
    trampoline 12
Trampoline13:
    trampoline 13
Trampoline14:
    trampoline 14
Trampoline15:
    trampoline 15

TrampolineX0:
    trampoline_vec 0
TrampolineX1:
    trampoline_vec 1
TrampolineX2:
    trampoline_vec 2
TrampolineX3:
    trampoline_vec 3
TrampolineX4:
    trampoline_vec 4
TrampolineX5:
    trampoline_vec 5
TrampolineX6:
    trampoline_vec 6
TrampolineX7:
    trampoline_vec 7
TrampolineX8:
    trampoline_vec 8
TrampolineX9:
    trampoline_vec 9
TrampolineX10:
    trampoline_vec 10
TrampolineX11:
    trampoline_vec 11
TrampolineX12:
    trampoline_vec 12
TrampolineX13:
    trampoline_vec 13
TrampolineX14:
    trampoline_vec 14
TrampolineX15:
    trampoline_vec 15

# When a callback is relayed, Koffi will call into Node.js and V8 to execute Javascript.
# The problem is that we're still running on the separate Koffi stack, and V8 will
# probably misdetect this as a "stack overflow". We have to restore the old
# stack pointer, call Node.js/V8 and go back to ours.
# The first three parameters (r0, r1, r2) are passed through untouched.
CallSwitchStack:
    .cfi_startproc
    push {fp, lr}
    .cfi_def_cfa sp, 8
    push {r4, r5}
    .cfi_def_cfa sp, 16
    .cfi_offset 11, 4
    .cfi_offset 14, 8
    mov fp, sp
    ldr r4, [sp, 16]
    ldr r5, [r4, 0]
    sub r5, sp, r5
    and r5, r5, #-16
    str r5, [r4, 4]
    ldr r4, [sp, 20]
    mov sp, r3
    blx r4
    mov sp, fp
    .cfi_def_cfa sp, 16
    pop {r4, r5}
    .cfi_def_cfa sp, 8
    pop {fp, lr}
    .cfi_def_cfa sp, 0
    .cfi_restore 11
    .cfi_restore 14
    bx lr
    .cfi_endproc
