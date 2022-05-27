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

    ; Copy function pointer to r9, in order to save it through argument forwarding.
    ; Save RSP in r29 (non-volatile), and use carefully assembled stack provided by caller.
    MACRO
    prologue

    stp x29, x30, [sp, -16]!
    mov x29, sp
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
    forward_int

    ldr x8, [x1, 64]
    ldr x7, [x1, 56]
    ldr x6, [x1, 48]
    ldr x5, [x1, 40]
    ldr x4, [x1, 32]
    ldr x3, [x1, 24]
    ldr x2, [x1, 16]
    ldr x0, [x1, 0]
    ldr x1, [x1, 8]
    MEND

    ; Prepare vector argument registers from array passed by caller.
    MACRO
    forward_vec

    ldr d7, [x1, 128]
    ldr d6, [x1, 120]
    ldr d5, [x1, 112]
    ldr d4, [x1, 104]
    ldr d3, [x1, 96]
    ldr d2, [x1, 88]
    ldr d1, [x1, 80]
    ldr d0, [x1, 72]
    MEND

ForwardCallGG PROC
    prologue
    forward_int
    epilogue
    ENDP

ForwardCallF PROC
    prologue
    forward_int
    epilogue
    ENDP

ForwardCallDDDD PROC
    prologue
    forward_int
    epilogue
    ENDP

ForwardCallXGG PROC
    prologue
    forward_vec
    forward_int
    epilogue
    ENDP

ForwardCallXF PROC
    prologue
    forward_vec
    forward_int
    epilogue
    ENDP

ForwardCallXDDDD PROC
    prologue
    forward_vec
    forward_int
    epilogue
    ENDP

    END
