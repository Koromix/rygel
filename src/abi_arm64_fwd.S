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
# Save RSP in r29 (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    stp x29, x30, [sp, -16]!
    .cfi_def_cfa sp, 16
    .cfi_offset 29, 16
    .cfi_offset 30, 8
    mov x29, sp
    .cfi_def_cfa x29, 16
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
.macro forward_int
    ldr x8, [x1, 64]
    ldr x7, [x1, 56]
    ldr x6, [x1, 48]
    ldr x5, [x1, 40]
    ldr x4, [x1, 32]
    ldr x3, [x1, 24]
    ldr x2, [x1, 16]
    ldr x0, [x1, 0]
    ldr x1, [x1, 8]
.endm

# Prepare vector argument registers from array passed by caller.
.macro forward_vec
    ldr d7, [x1, 128]
    ldr d6, [x1, 120]
    ldr d5, [x1, 112]
    ldr d4, [x1, 104]
    ldr d3, [x1, 96]
    ldr d2, [x1, 88]
    ldr d1, [x1, 80]
    ldr d0, [x1, 72]
.endm

SYMBOL(ForwardCallGG):
    prologue
    forward_int
    epilogue

SYMBOL(ForwardCallF):
    prologue
    forward_int
    epilogue

SYMBOL(ForwardCallDDDD):
    prologue
    forward_int
    epilogue

SYMBOL(ForwardCallXGG):
    prologue
    forward_vec
    forward_int
    epilogue

SYMBOL(ForwardCallXF):
    prologue
    forward_vec
    forward_int
    epilogue

SYMBOL(ForwardCallXDDDD):
    prologue
    forward_vec
    forward_int
    epilogue
