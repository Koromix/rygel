// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "parser.hh"
#include "run.hh"

namespace RG {

union Value {
    bool b;
    int64_t i;
    double d;
    const char *str;
};

void Run(const Program &program)
{
    HeapArray<Value> stack;

    for (Size pc = 0; pc < program.ir.len; pc++) {
        const Instruction &inst = program.ir[pc];

#ifndef NDEBUG
        switch (inst.code) {
            case Opcode::PushBool: { LogDebug("(0x%1) PushBool %2", FmtHex(pc).Pad0(-5), inst.u.b); } break;
            case Opcode::PushInt: { LogDebug("(0x%1) PushInt %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::PushDouble: { LogDebug("(0x%1) PushDouble %2", FmtHex(pc).Pad0(-5), inst.u.d); } break;
            case Opcode::PushString: { LogDebug("(0x%1) PushString %2", FmtHex(pc).Pad0(-5), inst.u.str); } break;
            case Opcode::Pop: { LogDebug("(0x%1) Pop %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

            case Opcode::StoreBool: { LogDebug("(0x%1) StoreBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::StoreInt: { LogDebug("(0x%1) StoreInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::StoreDouble: { LogDebug("(0x%1) StoreDouble @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::StoreString: { LogDebug("(0x%1) StoreString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::LoadBool: { LogDebug("(0x%1) LoadBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::LoadInt: { LogDebug("(0x%1) LoadInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::LoadDouble: { LogDebug("(0x%1) LoadDouble @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            case Opcode::LoadString: { LogDebug("(0x%1) LoadString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

            case Opcode::BranchIfTrue: { LogDebug("(0x%1) BranchIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;
            case Opcode::BranchIfFalse: { LogDebug("(0x%1) BranchIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;

            default: { LogDebug("(0x%1) %2", FmtHex(pc).Pad0(-5), OpcodeNames[(int)inst.code]); } break;
        }
#endif

        switch (inst.code) {
            case Opcode::PushBool: { stack.Append({.b = inst.u.b}); } break;
            case Opcode::PushInt: { stack.Append({.i = inst.u.i}); } break;
            case Opcode::PushDouble: { stack.Append({.d = inst.u.d}); } break;
            case Opcode::PushString: { stack.Append({.str = inst.u.str}); } break;
            case Opcode::Pop: { stack.RemoveLast(inst.u.i); } break;

            case Opcode::StoreBool: { stack[inst.u.i].b = stack[stack.len - 1].b; } break;
            case Opcode::StoreInt: { stack[inst.u.i].i = stack[stack.len - 1].i; } break;
            case Opcode::StoreDouble: { stack[inst.u.i].d = stack[stack.len - 1].d; } break;
            case Opcode::StoreString: { stack[inst.u.i].str = stack[stack.len - 1].str; } break;
            case Opcode::LoadBool: { stack.Append({.b = stack[inst.u.i].b}); } break;
            case Opcode::LoadInt: { stack.Append({.i = stack[inst.u.i].i}); } break;
            case Opcode::LoadDouble: { stack.Append({.d = stack[inst.u.i].d}); } break;
            case Opcode::LoadString: { stack.Append({.str = stack[inst.u.i].str}); } break;

            case Opcode::NegateInt: {
                int64_t i = stack[stack.len - 1].i;
                stack[stack.len - 1].i = -i;
            } break;
            case Opcode::AddInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 + i2;
            } break;
            case Opcode::SubstractInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 - i2;
            } break;
            case Opcode::MultiplyInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack.ptr[--stack.len - 1].i = i1 * i2;
            } break;
            case Opcode::DivideInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 / i2;
            } break;
            case Opcode::ModuloInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 % i2;
            } break;
            case Opcode::EqualInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 == i2);
            } break;
            case Opcode::NotEqualInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 != i2);
            } break;
            case Opcode::GreaterInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 > i2);
            } break;
            case Opcode::GreaterOrEqualInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 >= i2);
            } break;
            case Opcode::LessInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 < i2);
            } break;
            case Opcode::LessOrEqualInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 <= i2);
            } break;
            case Opcode::AndInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 & i2;
            } break;
            case Opcode::OrInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 | i2;
            } break;
            case Opcode::XorInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 ^ i2;
            } break;
            case Opcode::NotInt: {
                int64_t i = stack[stack.len - 1].i;
                stack[stack.len - 1].i = ~i;
            } break;
            case Opcode::LeftShiftInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 << i2;
            } break;
            case Opcode::RightShiftInt: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = (int64_t)((uint64_t)i1 >> i2);
            } break;

            case Opcode::NegateDouble: {
                double d = stack[stack.len - 1].d;
                stack[stack.len - 1].d = -d;
            } break;
            case Opcode::AddDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].d = d1 + d2;
            } break;
            case Opcode::SubstractDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].d = d1 - d2;
            } break;
            case Opcode::MultiplyDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].d = d1 * d2;
            } break;
            case Opcode::DivideDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].d = d1 / d2;
            } break;
            case Opcode::EqualDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].b = (d1 == d2);
            } break;
            case Opcode::NotEqualDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].b = (d1 != d2);
            } break;
            case Opcode::GreaterDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].b = (d1 > d2);
            } break;
            case Opcode::GreaterOrEqualDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].b = (d1 >= d2);
            } break;
            case Opcode::LessDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].b = (d1 < d2);
            } break;
            case Opcode::LessOrEqualDouble: {
                double d1 = stack[stack.len - 2].d;
                double d2 = stack[stack.len - 1].d;
                stack[--stack.len - 1].b = (d1 <= d2);
            } break;

            case Opcode::EqualBool: {
                bool b1 = stack[stack.len - 2].b;
                bool b2 = stack[stack.len - 1].b;
                stack[--stack.len - 1].b = (b1 == b2);
            } break;
            case Opcode::NotEqualBool: {
                bool b1 = stack[stack.len - 2].b;
                bool b2 = stack[stack.len - 1].b;
                stack[--stack.len - 1].b = (b1 != b2);
            } break;
            case Opcode::NotBool: {
                bool b = stack[stack.len - 1].b;
                stack[stack.len - 1].b = !b;
            } break;
            case Opcode::AndBool: {
                bool b1 = stack[stack.len - 2].b;
                bool b2 = stack[stack.len - 1].b;
                stack[--stack.len - 1].b = b1 && b2;
            } break;
            case Opcode::OrBool: {
                bool b1 = stack[stack.len - 2].b;
                bool b2 = stack[stack.len - 1].b;
                stack[--stack.len - 1].b = b1 || b2;
            } break;
            case Opcode::XorBool: {
                bool b1 = stack[stack.len - 2].b;
                bool b2 = stack[stack.len - 1].b;
                stack[--stack.len - 1].b = b1 ^ b2;
            } break;

            case Opcode::Jump: { pc = (Size)inst.u.i - 1; } break;
            case Opcode::BranchIfTrue: {
                bool b = stack[stack.len - 1].b;
                pc = b ? (Size)(inst.u.i - 1) : pc;
            } break;
            case Opcode::BranchIfFalse: {
                bool b = stack[stack.len - 1].b;
                pc = b ? pc : (Size)(inst.u.i - 1);
            } break;
        }
    }

    RG_ASSERT(stack.len == program.variables.len);

    for (const VariableInfo &var: program.variables) {
        switch (var.type) {
            case Type::Bool: { PrintLn("%1 (Bool) = %2", var.name, stack[var.offset].b); } break;
            case Type::Integer: { PrintLn("%1 (Integer) = %2", var.name, stack[var.offset].i); } break;
            case Type::Double: { PrintLn("%1 (Double) = %2", var.name, stack[var.offset].d); } break;
            case Type::String: { PrintLn("%1 (String) = '%2'", var.name, stack[var.offset].str); } break;
        }
    }
}

}
