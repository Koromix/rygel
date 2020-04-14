// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "parser.hh"
#include "vm.hh"

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
    Size pc = -1;

    for (;;) {
        const Instruction &inst = program.ir[++pc];

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

            case Opcode::Jump: { pc += (Size)inst.u.i - 1; } break;
            case Opcode::BranchIfTrue: {
                bool b = stack.ptr[--stack.len].b;
                pc += b ? (Size)(inst.u.i - 1) : 0;
            } break;
            case Opcode::BranchIfFalse: {
                bool b = stack.ptr[--stack.len].b;
                pc += b ? 0 : (Size)(inst.u.i - 1);
            } break;
            case Opcode::SkipIfTrue: {
                bool b = stack[stack.len - 1].b;
                pc += b ? (Size)(inst.u.i - 1) : 0;
            } break;
            case Opcode::SkipIfFalse: {
                bool b = stack[stack.len - 1].b;
                pc += b ? 0 : (Size)(inst.u.i - 1);
            } break;

            case Opcode::Exit: {
                RG_ASSERT(stack.len == program.variables.len);

                for (const VariableInfo &var: program.variables) {
                    switch (var.type) {
                        case Type::Bool: { PrintLn("%1 (Bool) = %2", var.name, stack[var.offset].b); } break;
                        case Type::Integer: { PrintLn("%1 (Integer) = %2", var.name, stack[var.offset].i); } break;
                        case Type::Double: { PrintLn("%1 (Double) = %2", var.name, stack[var.offset].d); } break;
                        case Type::String: { PrintLn("%1 (String) = '%2'", var.name, stack[var.offset].str); } break;
                    }
                }

                return;
            } break;
        }
    }
}

}
