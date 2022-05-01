// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

.global ForwardCallG
.global ForwardCallF
.global ForwardCallD
.global ForwardCallRG
.global ForwardCallRF
.global ForwardCallRD

// Copy function pointer to EAX, in order to save it through argument forwarding.
// Save ESP in EBX (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    .cfi_def_cfa esp, 4
    endbr32
    push %ebx
    .cfi_def_cfa esp, 8
    movl 8(%esp), %eax
    movl %esp, %ebx
    .cfi_def_cfa ebx, 8
    movl 12(%esp), %esp
.endm

.macro fastcall
    movl 0(%esp), %ecx
    movl 4(%esp), %edx
    addl $16, %esp
.endm

// Call native function.
// Once done, restore normal stack pointer and return.
// The return value is passed back untouched.
.macro epilogue
    call *%eax
    movl %ebx, %esp
    pop %ebx
    .cfi_def_cfa esp, 4
    ret
    .cfi_endproc
.endm

ForwardCallG:
    prologue
    epilogue

ForwardCallF:
    prologue
    epilogue

ForwardCallD:
    prologue
    epilogue

ForwardCallRG:
    prologue
    fastcall
    epilogue

ForwardCallRF:
    prologue
    fastcall
    epilogue

ForwardCallRD:
    prologue
    fastcall
    epilogue
