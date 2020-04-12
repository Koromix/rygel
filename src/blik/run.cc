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

    Value() {}
    Value(bool b) : b(b) {}
    Value(int64_t i) : i(i) {}
    Value(double d) : d(d) {}
    Value(const char *str) : str(str) {}
};

void Run(Span<const Instruction> ir)
{
    HeapArray<Value> stack;

    for (Size i = 0; i < ir.len; i++) {
        const Instruction &inst = ir[i];

#ifndef NDEBUG
        switch (inst.code) {
            case Opcode::PushBool: { LogDebug("(0x%1) PushBool %2", FmtHex(i).Pad0(-5), inst.u.b); } break;
            case Opcode::PushInt: { LogDebug("(0x%1) PushInt %2", FmtHex(i).Pad0(-5), inst.u.i); } break;
            case Opcode::PushDouble: { LogDebug("(0x%1) PushDouble %2", FmtHex(i).Pad0(-5), inst.u.d); } break;
            case Opcode::PushString: { LogDebug("(0x%1) PushString %2", FmtHex(i).Pad0(-5), inst.u.str); } break;

            case Opcode::BranchIfTrue: { LogDebug("(0x%1) BranchIfTrue 0x%2", FmtHex(i).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;
            case Opcode::BranchIfFalse: { LogDebug("(0x%1) BranchIfFalse 0x%2", FmtHex(i).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;

            default: { LogDebug("(0x%1) %2", FmtHex(i).Pad0(-5), OpcodeNames[(int)inst.code]); } break;
        }
#endif

        switch (inst.code) {
            case Opcode::PushBool: { stack.Append(Value(inst.u.b)); } break;
            case Opcode::PushInt: { stack.Append(Value((int64_t)inst.u.i)); } break;
            case Opcode::PushDouble: { stack.Append(Value(inst.u.d)); } break;
            case Opcode::PushString: { stack.Append(Value(inst.u.str)); } break;
            case Opcode::Pop: { stack.RemoveLast(1); } break;

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

            case Opcode::Jump: { i = (Size)inst.u.i - 1; } break;
            case Opcode::BranchIfTrue: {
                bool b = stack[stack.len - 1].b;
                i = b ? (Size)(inst.u.i - 1) : i;
            } break;
            case Opcode::BranchIfFalse: {
                bool b = stack[stack.len - 1].b;
                i = b ? i : (Size)(inst.u.i - 1);
            } break;
        }
    }

    RG_ASSERT(stack.len == 1);
}

}
