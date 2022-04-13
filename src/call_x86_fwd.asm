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

public ForwardCallG
public ForwardCallF
public ForwardCallD
public ForwardCallRG
public ForwardCallRF
public ForwardCallRD

.model flat, C
.code

; Copy function pointer to EAX, in order to save it through argument forwarding.
; Save ESP in EBX (non-volatile), and use carefully assembled stack provided by caller.
prologue macro
    endbr32
    push ebx
    mov ebx, esp
    mov eax, dword ptr [esp+8]
    mov esp, dword ptr [esp+12]
endm

fastcall macro
    mov ecx, dword ptr [esp+0]
    mov edx, dword ptr [esp+4]
    add esp, 8
endm

; Call native function.
; Once done, restore normal stack pointer and return.
; The return value is passed back untouched.
epilogue macro
    call eax
    mov esp, ebx
    pop ebx
    ret
endm

ForwardCallG proc
    prologue
    epilogue
ForwardCallG endp

ForwardCallF proc
    prologue
    epilogue
ForwardCallF endp

ForwardCallD proc
    prologue
    epilogue
ForwardCallD endp

ForwardCallRG proc
    prologue
    fastcall
    epilogue
ForwardCallRG endp

ForwardCallRF proc
    prologue
    fastcall
    epilogue
ForwardCallRF endp

ForwardCallRD proc
    prologue
    fastcall
    epilogue
ForwardCallRD endp

end
