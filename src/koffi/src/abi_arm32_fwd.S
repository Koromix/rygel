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

ForwardCallGG:
    prologue
    add r1, r1, #64
    ldmia r1, {r0-r3}
    epilogue

ForwardCallF:
    prologue
    add r1, r1, #64
    ldmia r1, {r0-r3}
    epilogue

ForwardCallDDDD:
    prologue
    add r1, r1, #64
    ldmia r1, {r0-r3}
    epilogue

ForwardCallXGG:
    prologue
    vldmia r1!, {d0-d7}
    ldmia r1, {r0-r3}
    epilogue

ForwardCallXF:
    prologue
    vldmia r1!, {d0-d7}
    ldmia r1, {r0-r3}
    epilogue

ForwardCallXDDDD:
    prologue
    vldmia r1!, {d0-d7}
    ldmia r1, {r0-r3}
    epilogue

# Callbacks
# ----------------------------

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
    add r12, sp, 64
    stmia r12!, {r0-r3}
    mov r0, \id
    mov r1, sp
    add r2, sp, #128
    mov r3, r12
    bl RelayCallback
    add sp, sp, #80
    ldmia sp!, {r0-r1}
    add sp, sp, #32
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
    mov r12, sp
    vstmia r12!, {d0-d7}
    stmia r12!, {r0-r3}
    mov r0, \id
    mov r1, sp
    add r2, sp, #128
    mov r3, r12
    bl RelayCallback
    add sp, sp, #80
    ldmia sp!, {r0-r1}
    vldmia sp!, {d0-d3}
    .cfi_def_cfa sp, 8
    pop {fp, lr}
    .cfi_def_cfa sp, 0
    .cfi_restore 11
    .cfi_restore 14
    bx lr
    .cfi_endproc
.endm

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

# Trampolines
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
.global Trampoline16
.global Trampoline17
.global Trampoline18
.global Trampoline19
.global Trampoline20
.global Trampoline21
.global Trampoline22
.global Trampoline23
.global Trampoline24
.global Trampoline25
.global Trampoline26
.global Trampoline27
.global Trampoline28
.global Trampoline29
.global Trampoline30
.global Trampoline31

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
.global TrampolineX16
.global TrampolineX17
.global TrampolineX18
.global TrampolineX19
.global TrampolineX20
.global TrampolineX21
.global TrampolineX22
.global TrampolineX23
.global TrampolineX24
.global TrampolineX25
.global TrampolineX26
.global TrampolineX27
.global TrampolineX28
.global TrampolineX29
.global TrampolineX30
.global TrampolineX31

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
Trampoline16:
    trampoline 16
Trampoline17:
    trampoline 17
Trampoline18:
    trampoline 18
Trampoline19:
    trampoline 19
Trampoline20:
    trampoline 20
Trampoline21:
    trampoline 21
Trampoline22:
    trampoline 22
Trampoline23:
    trampoline 23
Trampoline24:
    trampoline 24
Trampoline25:
    trampoline 25
Trampoline26:
    trampoline 26
Trampoline27:
    trampoline 27
Trampoline28:
    trampoline 28
Trampoline29:
    trampoline 29
Trampoline30:
    trampoline 30
Trampoline31:
    trampoline 31

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
TrampolineX16:
    trampoline_vec 16
TrampolineX17:
    trampoline_vec 17
TrampolineX18:
    trampoline_vec 18
TrampolineX19:
    trampoline_vec 19
TrampolineX20:
    trampoline_vec 20
TrampolineX21:
    trampoline_vec 21
TrampolineX22:
    trampoline_vec 22
TrampolineX23:
    trampoline_vec 23
TrampolineX24:
    trampoline_vec 24
TrampolineX25:
    trampoline_vec 25
TrampolineX26:
    trampoline_vec 26
TrampolineX27:
    trampoline_vec 27
TrampolineX28:
    trampoline_vec 28
TrampolineX29:
    trampoline_vec 29
TrampolineX30:
    trampoline_vec 30
TrampolineX31:
    trampoline_vec 31