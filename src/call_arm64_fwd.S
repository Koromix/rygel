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

// These five are the same, but they differ (in the C side) by their return type.
// Unlike the five next functions, these ones don't forward XMM argument registers.
.global ForwardCallGG
.global ForwardCallF
.global ForwardCallDDDD

// The X variants are slightly slower, and are used when XMM arguments must be forwarded.
.global ForwardCallXGG
.global ForwardCallXF
.global ForwardCallXDDDD

// Copy function pointer to r9, in order to save it through argument forwarding.
// Save RSP in r19 (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    stp x29, x30, [sp, -16]!
    .cfi_def_cfa sp, 16
    .cfi_offset 29, 16
    .cfi_offset 30, 8
    str x19, [sp, -16]!
    .cfi_def_cfa sp, 32
    mov x9, x0
    mov x19, sp
    .cfi_def_cfa x19, 32
    mov sp, x1
    add sp, sp, #192
.endm

// Call native function.
// Once done, restore normal stack pointer and return.
// The return value is passed untouched through r0, r1, v0 and/or v1.
.macro epilogue
    blr x9
    mov sp, x19
    .cfi_def_cfa sp, 32
    ldr x19, [sp], 16
    .cfi_def_cfa sp, 16
    ldp x29, x30, [sp], 16
    .cfi_def_cfa sp, 0
    .cfi_restore 30
    .cfi_restore 29
    ret
    .cfi_endproc
.endm

// Prepare general purpose argument registers from array passed by caller.
.macro forward_int
    ldr x8, [x1, 120]
    ldr x7, [x1, 112]
    ldr x6, [x1, 104]
    ldr x5, [x1, 96]
    ldr x4, [x1, 88]
    ldr x3, [x1, 80]
    ldr x2, [x1, 72]
    ldr x0, [x1, 56]
    ldr x1, [x1, 64]
.endm

// Prepare vector argument registers from array passed by caller.
.macro forward_vec
    ldr d7, [x1, 184]
    ldr d6, [x1, 176]
    ldr d5, [x1, 168]
    ldr d4, [x1, 160]
    ldr d3, [x1, 152]
    ldr d2, [x1, 144]
    ldr d1, [x1, 136]
    ldr d0, [x1, 128]
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
