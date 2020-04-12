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
            case Opcode::PushInteger: { LogDebug("(0x%1) PushInteger %2", FmtHex(i).Pad0(-5), inst.u.i); } break;
            case Opcode::PushDouble: { LogDebug("(0x%1) PushDouble %2", FmtHex(i).Pad0(-5), inst.u.d); } break;
            case Opcode::PushString: { LogDebug("(0x%1) PushString %2", FmtHex(i).Pad0(-5), inst.u.str); } break;

            case Opcode::BranchIfTrue: { LogDebug("(0x%1) BranchIfTrue 0x%2", FmtHex(i).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;
            case Opcode::BranchIfFalse: { LogDebug("(0x%1) BranchIfFalse 0x%2", FmtHex(i).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;

            default: { LogDebug("(0x%1) %2", FmtHex(i).Pad0(-5), OpcodeNames[(int)inst.code]); } break;
        }
#endif

        switch (inst.code) {
            case Opcode::PushBool: { stack.Append(Value(inst.u.b)); } break;
            case Opcode::PushInteger: { stack.Append(Value((int64_t)inst.u.i)); } break;
            case Opcode::PushDouble: { stack.Append(Value(inst.u.d)); } break;
            case Opcode::PushString: { stack.Append(Value(inst.u.str)); } break;
            case Opcode::Pop: { stack.RemoveLast(1); } break;

            case Opcode::Add: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 + i2;
            } break;
            case Opcode::Substract: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 - i2;
            } break;
            case Opcode::Multiply: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack.ptr[--stack.len - 1].i = i1 * i2;
            } break;
            case Opcode::Divide: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 / i2;
            } break;
            case Opcode::Modulo: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 % i2;
            } break;
            case Opcode::Equal: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 == i2);
            } break;
            case Opcode::NotEqual: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 != i2);
            } break;
            case Opcode::Greater: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 > i2);
            } break;
            case Opcode::GreaterOrEqual: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 >= i2);
            } break;
            case Opcode::Less: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 < i2);
            } break;
            case Opcode::LessOrEqual: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].b = (i1 <= i2);
            } break;
            case Opcode::And: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 & i2;
            } break;
            case Opcode::Or: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 | i2;
            } break;
            case Opcode::Xor: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 ^ i2;
            } break;
            case Opcode::Not: {
                int64_t i = stack[stack.len - 1].i;
                stack[stack.len - 1].i = ~i;
            } break;
            case Opcode::LeftShift: {
                int64_t i1 = stack[stack.len - 2].i;
                int64_t i2 = stack[stack.len - 1].i;
                stack[--stack.len - 1].i = i1 << i2;
            } break;
            case Opcode::RightShift: {
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
            case Opcode::LogicNot: {
                bool b = stack[stack.len - 1].b;
                stack[stack.len - 1].b = !b;
            } break;
            case Opcode::LogicAnd: {
                bool b1 = stack[stack.len - 2].b;
                bool b2 = stack[stack.len - 1].b;
                stack[--stack.len - 1].b = b1 && b2;
            } break;
            case Opcode::LogicOr: {
                bool b1 = stack[stack.len - 2].b;
                bool b2 = stack[stack.len - 1].b;
                stack[--stack.len - 1].b = b1 || b2;
            } break;
            case Opcode::LogicXor: {
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
