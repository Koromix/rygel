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

; Forward
; ----------------------------

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
    mov eax, dword ptr [esp+16]
    mov dword ptr [eax+0], esp
    mov eax, dword ptr [esp+8]
    mov esp, dword ptr [esp+12]
endm

fastcall macro
    mov ecx, dword ptr [esp+0]
    mov edx, dword ptr [esp+4]
    add esp, 16
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

; Callback trampolines
; ----------------------------

public Trampoline0
public Trampoline1
public Trampoline2
public Trampoline3
public Trampoline4
public Trampoline5
public Trampoline6
public Trampoline7
public Trampoline8
public Trampoline9
public Trampoline10
public Trampoline11
public Trampoline12
public Trampoline13
public Trampoline14
public Trampoline15
public TrampolineX0
public TrampolineX1
public TrampolineX2
public TrampolineX3
public TrampolineX4
public TrampolineX5
public TrampolineX6
public TrampolineX7
public TrampolineX8
public TrampolineX9
public TrampolineX10
public TrampolineX11
public TrampolineX12
public TrampolineX13
public TrampolineX14
public TrampolineX15
extern RelayCallBack : PROC
public CallSwitchStack

trampoline macro ID
    endbr32
    sub esp, 44
    mov dword ptr [esp+0], ID
    mov dword ptr [esp+4], esp
    lea eax, dword ptr [esp+48]
    mov dword ptr [esp+8], eax
    lea eax, dword ptr [esp+16]
    mov dword ptr [esp+12], eax
    call RelayCallBack
    mov eax, dword ptr[esp+16]
    mov edx, dword ptr[esp+20]
    add esp, 44
    ret
endm

trampoline_x87 macro ID
    local l1, l2, l3

    endbr32
    sub esp, 44
    mov dword ptr [esp+0], ID
    mov dword ptr [esp+4], esp
    lea eax, dword ptr [esp+48]
    mov dword ptr [esp+8], eax
    lea eax, dword ptr [esp+16]
    mov dword ptr [esp+12], eax
    call RelayCallBack
    cmp byte ptr[esp+36], 1
    je l2
l1:
    fld dword ptr [esp+32]
    jmp l3
l2:
    fld qword ptr [esp+24]
l3:
    mov eax, dword ptr[esp+16]
    mov edx, dword ptr[esp+20]
    add esp, 44
    ret
endm

Trampoline0 proc
    trampoline 0
Trampoline0 endp
Trampoline1 proc
    trampoline 1
Trampoline1 endp
Trampoline2 proc
    trampoline 2
Trampoline2 endp
Trampoline3 proc
    trampoline 3
Trampoline3 endp
Trampoline4 proc
    trampoline 4
Trampoline4 endp
Trampoline5 proc
    trampoline 5
Trampoline5 endp
Trampoline6 proc
    trampoline 6
Trampoline6 endp
Trampoline7 proc
    trampoline 7
Trampoline7 endp
Trampoline8 proc
    trampoline 8
Trampoline8 endp
Trampoline9 proc
    trampoline 9
Trampoline9 endp
Trampoline10 proc
    trampoline 10
Trampoline10 endp
Trampoline11 proc
    trampoline 11
Trampoline11 endp
Trampoline12 proc
    trampoline 12
Trampoline12 endp
Trampoline13 proc
    trampoline 13
Trampoline13 endp
Trampoline14 proc
    trampoline 14
Trampoline14 endp
Trampoline15 proc
    trampoline 15
Trampoline15 endp

TrampolineX0 proc
    trampoline_x87 0
TrampolineX0 endp
TrampolineX1 proc
    trampoline_x87 1
TrampolineX1 endp
TrampolineX2 proc
    trampoline_x87 2
TrampolineX2 endp
TrampolineX3 proc
    trampoline_x87 3
TrampolineX3 endp
TrampolineX4 proc
    trampoline_x87 4
TrampolineX4 endp
TrampolineX5 proc
    trampoline_x87 5
TrampolineX5 endp
TrampolineX6 proc
    trampoline_x87 6
TrampolineX6 endp
TrampolineX7 proc
    trampoline_x87 7
TrampolineX7 endp
TrampolineX8 proc
    trampoline_x87 8
TrampolineX8 endp
TrampolineX9 proc
    trampoline_x87 9
TrampolineX9 endp
TrampolineX10 proc
    trampoline_x87 10
TrampolineX10 endp
TrampolineX11 proc
    trampoline_x87 11
TrampolineX11 endp
TrampolineX12 proc
    trampoline_x87 12
TrampolineX12 endp
TrampolineX13 proc
    trampoline_x87 13
TrampolineX13 endp
TrampolineX14 proc
    trampoline_x87 14
TrampolineX14 endp
TrampolineX15 proc
    trampoline_x87 15
TrampolineX15 endp

CallSwitchStack proc
    endbr32
    push ebx
    mov ebx, esp
    mov edx, dword ptr [esp+28]
    mov ecx, dword ptr [esp+24]
    mov eax, esp
    sub eax, dword ptr [ecx+0]
    and eax, -16
    mov dword ptr [ecx+4], eax
    mov esp, dword ptr [esp+20]
    sub esp, 28
    mov eax, dword ptr [ebx+8]
    mov dword ptr [esp+0], eax
    mov eax, dword ptr [ebx+12]
    mov dword ptr [esp+4], eax
    mov eax, dword ptr [ebx+16]
    mov dword ptr [esp+8], eax
    call edx
    mov esp, ebx
    pop ebx
    ret
CallSwitchStack endp

end
