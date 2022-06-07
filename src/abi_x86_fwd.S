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

.global ForwardCallG
.global ForwardCallF
.global ForwardCallD
.global ForwardCallRG
.global ForwardCallRF
.global ForwardCallRD

# Copy function pointer to EAX, in order to save it through argument forwarding.
# Also make a copy of the SP to CallData::old_sp because the callback system might need it.
# Save ESP in EBX (non-volatile), and use carefully assembled stack provided by caller.
.macro prologue
    .cfi_startproc
    .cfi_def_cfa esp, 4
    endbr32
    push %ebx
    .cfi_def_cfa esp, 8
    movl %esp, %ebx
    .cfi_def_cfa ebx, 8
    movl 16(%esp), %eax
    movl %esp, 0(%eax)
    movl 8(%esp), %eax
    movl 12(%esp), %esp
.endm

.macro fastcall
    movl 0(%esp), %ecx
    movl 4(%esp), %edx
    addl $16, %esp
.endm

# Call native function.
# Once done, restore normal stack pointer and return.
# The return value is passed back untouched.
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

# Callback trampolines
# ----------------------------

.global Trampoline0
.global Trampoline1
.global Trampoline2
.global Trampoline3
.global Trampoline4
.global Trampoline5
.global Trampoline6
.global Trampoline7
.global Trampoline8
.global Trampoline9
.global Trampoline10
.global Trampoline11
.global Trampoline12
.global Trampoline13
.global Trampoline14
.global Trampoline15
.global TrampolineX0
.global TrampolineX1
.global TrampolineX2
.global TrampolineX3
.global TrampolineX4
.global TrampolineX5
.global TrampolineX6
.global TrampolineX7
.global TrampolineX8
.global TrampolineX9
.global TrampolineX10
.global TrampolineX11
.global TrampolineX12
.global TrampolineX13
.global TrampolineX14
.global TrampolineX15
.global RelayCallBack
.global CallSwitchStack

# Call the C function RelayCallBack with the following arguments:
# static trampoline ID, the current stack pointer, a pointer to the stack arguments of this call,
# and a pointer to a struct that will contain the result registers.
# After the call, simply load these registers from the output struct.
.macro trampoline id
    .cfi_startproc
    .cfi_def_cfa esp, 4
    endbr32
    sub $44, %esp
    .cfi_def_cfa esp, 48
    movl $\id, 0(%esp)
    movl %esp, 4(%esp)
    leal 48(%esp), %eax
    movl %eax, 8(%esp)
    leal 16(%esp), %eax
    movl %eax, 12(%esp)
    call GetEIP
    addl $_GLOBAL_OFFSET_TABLE_, %ecx
    call *RelayCallBack@GOT(%ecx)
    movl 16(%esp), %eax
    movl 20(%esp), %edx
    addl $44, %esp
    .cfi_def_cfa esp, 4
    ret
    .cfi_endproc
.endm

# This version also loads the x87 stack with the result, if need be.
# We have to branch to avoid x87 stack imbalance.
.macro trampoline_x87 id
    .cfi_startproc
    .cfi_def_cfa esp, 4
    endbr32
    sub $44, %esp
    .cfi_def_cfa esp, 48
    movl $\id, 0(%esp)
    movl %esp, 4(%esp)
    leal 48(%esp), %eax
    movl %eax, 8(%esp)
    leal 16(%esp), %eax
    movl %eax, 12(%esp)
    call GetEIP
    addl $_GLOBAL_OFFSET_TABLE_, %ecx
    call *RelayCallBack@GOT(%ecx)
    cmpb $1, 36(%esp)
    je 2f
1:
    flds 32(%esp)
    jmp 3f
2:
    fldl 24(%esp)
3:
    movl 16(%esp), %eax
    movl 20(%esp), %edx
    addl $44, %esp
    .cfi_def_cfa esp, 4
    ret
    .cfi_endproc
.endm

GetEIP:
    movl (%esp), %ecx
    ret

Trampoline0:
    trampoline 0
Trampoline1:
    trampoline 1
Trampoline2:
    trampoline 2
Trampoline3:
    trampoline 3
Trampoline4:
    trampoline 4
Trampoline5:
    trampoline 5
Trampoline6:
    trampoline 6
Trampoline7:
    trampoline 7
Trampoline8:
    trampoline 8
Trampoline9:
    trampoline 9
Trampoline10:
    trampoline 10
Trampoline11:
    trampoline 11
Trampoline12:
    trampoline 12
Trampoline13:
    trampoline 13
Trampoline14:
    trampoline 14
Trampoline15:
    trampoline 15

TrampolineX0:
    trampoline_x87 0
TrampolineX1:
    trampoline_x87 1
TrampolineX2:
    trampoline_x87 2
TrampolineX3:
    trampoline_x87 3
TrampolineX4:
    trampoline_x87 4
TrampolineX5:
    trampoline_x87 5
TrampolineX6:
    trampoline_x87 6
TrampolineX7:
    trampoline_x87 7
TrampolineX8:
    trampoline_x87 8
TrampolineX9:
    trampoline_x87 9
TrampolineX10:
    trampoline_x87 10
TrampolineX11:
    trampoline_x87 11
TrampolineX12:
    trampoline_x87 12
TrampolineX13:
    trampoline_x87 13
TrampolineX14:
    trampoline_x87 14
TrampolineX15:
    trampoline_x87 15

# When a callback is relayed, Koffi will call into Node.js and V8 to execute Javascript.
# The problem is that we're still running on the separate Koffi stack, and V8 will
# probably misdetect this as a "stack overflow". We have to restore the old
# stack pointer, call Node.js/V8 and go back to ours.
CallSwitchStack:
    .cfi_startproc
    .cfi_def_cfa esp, 4
    endbr32
    push %ebx
    .cfi_def_cfa esp, 8
    movl %esp, %ebx
    .cfi_def_cfa ebx, 8
    movl 28(%esp), %edx
    movl 24(%esp), %ecx
    movl %esp, %eax
    subl 0(%ecx), %eax
    andl $-16, %eax
    movl %eax, 4(%ecx)
    movl 20(%esp), %esp
    subl $16, %esp
    movl 8(%ebx), %eax
    movl %eax, 0(%esp)
    movl 12(%ebx), %eax
    movl %eax, 4(%esp)
    movl 16(%ebx), %eax
    movl %eax, 8(%esp)
    call *%edx
    mov %ebx, %esp
    .cfi_def_cfa esp, 8
    pop %ebx
    .cfi_def_cfa esp, 4
    ret
    .cfi_endproc
