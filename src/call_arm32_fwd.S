// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

.syntax unified

// These three are the same, but they differ (in the C side) by their return type.
// Unlike the three next functions, these ones don't forward XMM argument registers.
.global ForwardCallGG
.global ForwardCallF
.global ForwardCallDDDD

// The X variants are slightly slower, and are used when XMM arguments must be forwarded.
.global ForwardCallXGG
.global ForwardCallXF
.global ForwardCallXDDDD

// Copy function pointer to r12, in order to save it through argument forwarding.
// Save RSP in fp (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    push {fp, lr}
    .cfi_def_cfa sp, 8
    .cfi_offset 11, 4
    .cfi_offset 14, 8
    mov fp, sp
    .cfi_def_cfa fp, 8
    mov r12, r0
    mov sp, r1
    add sp, sp, #80
.endm

// Call native function.
// Once done, restore normal stack pointer and return.
// The return value is passed untouched through r0, r1, and or FP registers
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

// Prepare general purpose argument registers from array passed by caller.
.macro forward_int
    ldr r3, [r1, 76]
    ldr r2, [r1, 72]
    ldr r0, [r1, 64]
    ldr r1, [r1, 68]
.endm

// Prepare vector argument registers from array passed by caller.
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
