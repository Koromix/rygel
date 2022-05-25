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

// These three are the same, but they differ (in the C side) by their return type.
// Unlike the three next functions, these ones don't forward FA argument registers.
.global ForwardCallGG
.global ForwardCallF
.global ForwardCallDG
.global ForwardCallGD
.global ForwardCallDD

// The X variants are slightly slower, and are used when FA arguments must be forwarded.
.global ForwardCallXGG
.global ForwardCallXF
.global ForwardCallXDG
.global ForwardCallXGD
.global ForwardCallXDD

// Copy function pointer to t0, in order to save it through argument forwarding.
// Save SP in s1, and use carefully assembled stack provided by caller.
.macro prologue
    addi sp, sp, -16
    mv t0, a0
    sd ra, 0(sp)
    sd s1, 8(sp)
    mv s1, sp
    addi sp, a1, 128
.endm

// Call native function.
// Once done, restore normal stack pointer and return.
// The return value is passed untouched through registers.
.macro epilogue
    jalr t0
    mv sp, s1
    ld ra, 0(sp)
    ld s1, 8(sp)
    addi sp, sp, 16
    ret
.endm

// Prepare general purpose argument registers from array passed by caller.
.macro forward_int
    ld a7, 120(a1)
    ld a6, 112(a1)
    ld a5, 104(a1)
    ld a4, 96(a1)
    ld a3, 88(a1)
    ld a2, 80(a1)
    ld a0, 64(a1)
    ld a1, 72(a1)
.endm

// Prepare vector argument registers from array passed by caller.
.macro forward_vec
    fld fa7, 56(a1)
    fld fa6, 48(a1)
    fld fa5, 40(a1)
    fld fa4, 32(a1)
    fld fa3, 24(a1)
    fld fa2, 16(a1)
    fld fa1, 8(a1)
    fld fa0, 0(a1)
.endm

ForwardCallGG:
    prologue
    forward_int
    epilogue

ForwardCallF:
    prologue
    forward_int
    epilogue

ForwardCallDG:
    prologue
    forward_int
    epilogue

ForwardCallGD:
    prologue
    forward_int
    epilogue

ForwardCallDD:
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

ForwardCallXDG:
    prologue
    forward_vec
    forward_int
    epilogue

ForwardCallXGD:
    prologue
    forward_vec
    forward_int
    epilogue

ForwardCallXDD:
    prologue
    forward_vec
    forward_int
    epilogue
