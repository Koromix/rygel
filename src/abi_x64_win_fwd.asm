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

; These three are the same, but they differ (in the C side) by their return type.
; Unlike the three next functions, these ones don't forward XMM argument registers.
public ForwardCallG
public ForwardCallF
public ForwardCallD

; The X variants are slightly slower, and are used when XMM arguments must be forwarded.
public ForwardCallXG
public ForwardCallXF
public ForwardCallXD

.code

; Copy function pointer to RAX, in order to save it through argument forwarding.
; Save RSP in RBX (non-volatile), and use carefully assembled stack provided by caller.
prologue macro
    endbr64
    mov rax, rcx
    push rbx
    .pushreg rbx
    mov rbx, rsp
    .setframe rbx, 0
    .endprolog
    mov rsp, rdx
endm

; Call native function.
; Once done, restore normal stack pointer and return.
; The return value is passed untouched through RAX or XMM0.
epilogue macro
    call rax
    mov rsp, rbx
    pop rbx
    ret
endm

; Prepare integer argument registers from array passed by caller.
forward_int macro
    mov r9, qword ptr [rdx+24]
    mov r8, qword ptr [rdx+16]
    mov rcx, qword ptr [rdx+0]
    mov rdx, qword ptr [rdx+8]
endm

; Prepare XMM argument registers from array passed by caller.
forward_xmm macro
    movsd xmm3, qword ptr [rdx+24]
    movsd xmm2, qword ptr [rdx+16]
    movsd xmm1, qword ptr [rdx+8]
    movsd xmm0, qword ptr [rdx+0]
endm

ForwardCallG proc frame
    prologue
    forward_int
    epilogue
ForwardCallG endp

ForwardCallF proc frame
    prologue
    forward_int
    epilogue
ForwardCallF endp

ForwardCallD proc frame
    prologue
    forward_int
    epilogue
ForwardCallD endp

ForwardCallXG proc frame
    prologue
    forward_xmm
    forward_int
    epilogue
ForwardCallXG endp

ForwardCallXF proc frame
    prologue
    forward_xmm
    forward_int
    epilogue
ForwardCallXF endp

ForwardCallXD proc frame
    prologue
    forward_xmm
    forward_int
    epilogue
ForwardCallXD endp

end
