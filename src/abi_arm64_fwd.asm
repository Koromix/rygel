; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU Affero General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU Affero General Public License for more details.
;
; You should have received a copy of the GNU Affero General Public License
; along with this program. If not, see https://www.gnu.org/licenses/.

    AREA |.text|, CODE

; Forward
; ----------------------------

    ; These three are the same, but they differ (in the C side) by their return type.
    ; Unlike the three next functions, these ones don't forward XMM argument registers.
    EXPORT ForwardCallGG
    EXPORT ForwardCallF
    EXPORT ForwardCallDDDD

    ; The X variants are slightly slower, and are used when XMM arguments must be forwarded.
    EXPORT ForwardCallXGG
    EXPORT ForwardCallXF
    EXPORT ForwardCallXDDDD

    ; Copy function pointer to r9, in order to save it through argument forwarding.
    ; Save RSP in r29 (non-volatile), and use carefully assembled stack provided by caller.
    MACRO
    prologue

    stp x29, x30, [sp, -16]!
    mov x29, sp
    str x29, [x2, 0]
    mov x9, x0
    add sp, x1, #136
    MEND

    ; Call native function.
    ; Once done, restore normal stack pointer and return.
    ; The return value is passed untouched through r0, r1, v0 and/or v1.
    MACRO
    epilogue

    blr x9
    mov sp, x29
    ldp x29, x30, [sp], 16
    ret
    MEND

    ; Prepare general purpose argument registers from array passed by caller.
    MACRO
    forward_gpr

    ldr x8, [x1, 64]
    ldp x6, x7, [x1, 48]
    ldp x4, x5, [x1, 32]
    ldp x2, x3, [x1, 16]
    ldp x0, x1, [x1, 0]
    MEND

    ; Prepare vector argument registers from array passed by caller.
    MACRO
    forward_vec

    ldp d6, d7, [x1, 120]
    ldp d4, d5, [x1, 104]
    ldp d2, d3, [x1, 88]
    ldp d0, d1, [x1, 72]
    MEND

ForwardCallGG PROC
    prologue
    forward_gpr
    epilogue
    ENDP

ForwardCallF PROC
    prologue
    forward_gpr
    epilogue
    ENDP

ForwardCallDDDD PROC
    prologue
    forward_gpr
    epilogue
    ENDP

ForwardCallXGG PROC
    prologue
    forward_vec
    forward_gpr
    epilogue
    ENDP

ForwardCallXF PROC
    prologue
    forward_vec
    forward_gpr
    epilogue
    ENDP

ForwardCallXDDDD PROC
    prologue
    forward_vec
    forward_gpr
    epilogue
    ENDP

; Callback trampolines
; ----------------------------

    EXPORT Trampoline0
    EXPORT Trampoline1
    EXPORT Trampoline2
    EXPORT Trampoline3
    EXPORT Trampoline4
    EXPORT Trampoline5
    EXPORT Trampoline6
    EXPORT Trampoline7
    EXPORT Trampoline8
    EXPORT Trampoline9
    EXPORT Trampoline10
    EXPORT Trampoline11
    EXPORT Trampoline12
    EXPORT Trampoline13
    EXPORT Trampoline14
    EXPORT Trampoline15
    EXPORT TrampolineX0
    EXPORT TrampolineX1
    EXPORT TrampolineX2
    EXPORT TrampolineX3
    EXPORT TrampolineX4
    EXPORT TrampolineX5
    EXPORT TrampolineX6
    EXPORT TrampolineX7
    EXPORT TrampolineX8
    EXPORT TrampolineX9
    EXPORT TrampolineX10
    EXPORT TrampolineX11
    EXPORT TrampolineX12
    EXPORT TrampolineX13
    EXPORT TrampolineX14
    EXPORT TrampolineX15
    EXPORT RelayCallback
    EXTERN RelayCallback
    EXPORT CallSwitchStack

    ; First, make a copy of the GPR argument registers (x0 to x7).
    ; Then call the C function RelayCallback with the following arguments:
    ; static trampoline ID, a pointer to the saved GPR array, a pointer to the stack
    ; arguments of this call, and a pointer to a struct that will contain the result registers.
    ; After the call, simply load these registers from the output struct.
    MACRO
    trampoline $ID

    stp x29, x30, [sp, -16]!
    sub sp, sp, #192
    stp x0, x1, [sp, 0]
    stp x2, x3, [sp, 16]
    stp x4, x5, [sp, 32]
    stp x6, x7, [sp, 48]
    str x8, [sp, 64]
    mov x0, $ID
    mov x1, sp
    add x2, sp, #208
    add x3, sp, #136
    bl RelayCallback
    ldp x0, x1, [sp, 136]
    add sp, sp, #192
    ldp x29, x30, [sp], 16
    ret
    MEND

    ; Same thing, but also forwards the floating-point argument registers and loads them at the end.
    MACRO
    trampoline_vec $ID

    stp x29, x30, [sp, -16]!
    sub sp, sp, #192
    stp x0, x1, [sp, 0]
    stp x2, x3, [sp, 16]
    stp x4, x5, [sp, 32]
    stp x6, x7, [sp, 48]
    str x8, [sp, 64]
    stp d0, d1, [sp, 72]
    stp d2, d3, [sp, 88]
    stp d4, d5, [sp, 104]
    stp d6, d7, [sp, 120]
    mov x0, $ID
    mov x1, sp
    add x2, sp, #208
    add x3, sp, #136
    bl RelayCallback
    ldp x0, x1, [sp, 136]
    ldp d0, d1, [sp, 152]
    ldp d2, d3, [sp, 168]
    add sp, sp, #192
    ldp x29, x30, [sp], 16
    ret
    MEND

Trampoline0 PROC
    trampoline 0
    ENDP
Trampoline1 PROC
    trampoline 1
    ENDP
Trampoline2 PROC
    trampoline 2
    ENDP
Trampoline3 PROC
    trampoline 3
    ENDP
Trampoline4 PROC
    trampoline 4
    ENDP
Trampoline5 PROC
    trampoline 5
    ENDP
Trampoline6 PROC
    trampoline 6
    ENDP
Trampoline7 PROC
    trampoline 7
    ENDP
Trampoline8 PROC
    trampoline 8
    ENDP
Trampoline9 PROC
    trampoline 9
    ENDP
Trampoline10 PROC
    trampoline 10
    ENDP
Trampoline11 PROC
    trampoline 11
    ENDP
Trampoline12 PROC
    trampoline 12
    ENDP
Trampoline13 PROC
    trampoline 13
    ENDP
Trampoline14 PROC
    trampoline 14
    ENDP
Trampoline15 PROC
    trampoline 15
    ENDP

TrampolineX0 PROC
    trampoline_vec 0
    ENDP
TrampolineX1 PROC
    trampoline_vec 1
    ENDP
TrampolineX2 PROC
    trampoline_vec 2
    ENDP
TrampolineX3 PROC
    trampoline_vec 3
    ENDP
TrampolineX4 PROC
    trampoline_vec 4
    ENDP
TrampolineX5 PROC
    trampoline_vec 5
    ENDP
TrampolineX6 PROC
    trampoline_vec 6
    ENDP
TrampolineX7 PROC
    trampoline_vec 7
    ENDP
TrampolineX8 PROC
    trampoline_vec 8
    ENDP
TrampolineX9 PROC
    trampoline_vec 9
    ENDP
TrampolineX10 PROC
    trampoline_vec 10
    ENDP
TrampolineX11 PROC
    trampoline_vec 11
    ENDP
TrampolineX12 PROC
    trampoline_vec 12
    ENDP
TrampolineX13 PROC
    trampoline_vec 13
    ENDP
TrampolineX14 PROC
    trampoline_vec 14
    ENDP
TrampolineX15 PROC
    trampoline_vec 15
    ENDP

; When a callback is relayed, Koffi will call into Node.js and V8 to execute Javascript.
; The problem is that we're still running on the separate Koffi stack, and V8 will
; probably misdetect this as a "stack overflow". We have to restore the old
; stack pointer, call Node.js/V8 and go back to ours.
; The first three parameters (x0, x1, x2) are passed through untouched.
CallSwitchStack PROC
    stp x29, x30, [sp, -16]!
    mov x29, sp
    ldr x9, [x4, 0]
    sub x9, sp, x9
    and x9, x9, #-16
    str x9, [x4, 8]
    mov sp, x3
    blr x5
    mov sp, x29
    ldp x29, x30, [sp], 16
    ret
    ENDP

    END
