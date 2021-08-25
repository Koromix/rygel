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
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "../../core/libcc/libcc.hh"
#include "program.hh"
#include "vm.hh"

namespace RG {

bk_VirtualMachine::bk_VirtualMachine(const bk_Program *const program)
    : program(program)
{
    frames.AppendDefault();
}

bool bk_VirtualMachine::Run(bool debug)
{
    const bk_Instruction *inst;

    ir = program->ir;
    run = true;
    error = false;

    bk_CallFrame *frame = &frames[frames.len - 1];
    Size pc = frame->pc;
    Size bp = frame->bp;

    // Save PC on exit
    RG_DEFER { frame->pc = pc; };

    RG_ASSERT(pc < ir.len);

#if defined(__GNUC__) || defined(__clang__)
    static const void *dispatch[] = {
        #define OPCODE(Code) && Code,
        #include "opcodes.inc"
    };

    #define DISPATCH(PC) \
        inst = &ir[(PC)]; \
        if (debug) { \
            DumpInstruction(pc); \
        } \
        goto *dispatch[(int)inst->code];
    #define LOOP \
        DISPATCH(pc)
    #define CASE(Code) \
        Code
#else
    #define DISPATCH(PC) \
        inst = &ir[(PC)]; \
        if (debug) { \
            DumpInstruction(pc); \
        } \
        break
    #define LOOP \
        inst = &ir[pc]; \
        if (debug) { \
            DumpInstruction(pc); \
        } \
        for (;;) \
            switch(inst->code)
    #define CASE(Code) \
        case bk_Opcode::Code
#endif

    LOOP {
        CASE(Nop): {
            DISPATCH(++pc);
        }

        CASE(Push): {
            stack.Append({.i = inst->u.i});
            DISPATCH(++pc);
        }
        CASE(Pop): {
            stack.RemoveLast(inst->u.i);
            DISPATCH(++pc);
        }

        CASE(Load): {
            stack.Append({.i = stack[inst->u.i].i});
            DISPATCH(++pc);
        }
        CASE(LoadLocal): {
            stack.Append({.i = stack[bp + inst->u.i].i});
            DISPATCH(++pc);
        }
        CASE(LoadRef): {
            Size ptr = stack[stack.len - 2].i;
            Size idx = stack[stack.len - 1].i;
            stack.len -= 2;
            stack.Append({.i = stack[ptr + idx].i});
            DISPATCH(++pc);
        }
        CASE(LoadIndirect): {
            Size ptr = stack[stack.len - 2].i;
            Size idx = stack[stack.len - 1].i;
            stack.Append({.i = stack[ptr + idx].i});
            DISPATCH(++pc);
        }
        CASE(Lea): {
            stack.Append({.i = inst->u.i});
            DISPATCH(++pc);
        }
        CASE(LeaLocal): {
            stack.Append({.i = bp + inst->u.i});
            DISPATCH(++pc);
        }
        CASE(Store): {
            stack[inst->u.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreLocal): {
            stack[bp + inst->u.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreArray): {
            Size ptr = stack[stack.len - 3].i;
            Size idx = stack[stack.len - 2].i;
            stack[ptr + idx].i = stack[stack.len - 1].i;
            stack.len -= 3;
            DISPATCH(++pc);
        }
        CASE(Copy): {
            stack[inst->u.i].i = stack[stack.len - 1].i;
            DISPATCH(++pc);
        }
        CASE(CopyLocal): {
            stack[bp + inst->u.i].i = stack.ptr[stack.len - 1].i;
            DISPATCH(++pc);
        }
        CASE(CopyArray): {
            Size ptr = stack[stack.len - 3].i;
            Size idx = stack[stack.len - 2].i;
            stack[ptr + idx].i = stack[stack.len - 1].i;
            stack[stack.len - 3] = stack[stack.len - 1];
            stack.len -= 2;
            DISPATCH(++pc);
        }
        CASE(CheckIndex): {
            Size idx = stack[stack.len - 1].i;
            if (RG_UNLIKELY(idx < 0 || idx >= inst->u.i)) {
                frame->pc = pc;
                FatalError("Index is out of range: %1 (array length %2)", idx, inst->u.i);
                return false;
            }
            DISPATCH(++pc);
        }

        CASE(NegateInt): {
            int64_t i = stack[stack.len - 1].i;
            stack[stack.len - 1].i = -i;
            DISPATCH(++pc);
        }
        CASE(AddInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].i = i1 + i2;
            DISPATCH(++pc);
        }
        CASE(SubstractInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].i = i1 - i2;
            DISPATCH(++pc);
        }
        CASE(MultiplyInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack.ptr[--stack.len - 1].i = i1 * i2;
            DISPATCH(++pc);
        }
        CASE(DivideInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            if (RG_UNLIKELY(i2 == 0)) {
                frame->pc = pc;
                FatalError("Integer division by 0 is illegal");
                return false;
            }
            stack[--stack.len - 1].i = i1 / i2;
            DISPATCH(++pc);
        }
        CASE(ModuloInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            if (RG_UNLIKELY(i2 == 0)) {
                frame->pc = pc;
                FatalError("Integer division by 0 is illegal");
                return false;
            }
            stack[--stack.len - 1].i = i1 % i2;
            DISPATCH(++pc);
        }
        CASE(EqualInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 == i2);
            DISPATCH(++pc);
        }
        CASE(NotEqualInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 != i2);
            DISPATCH(++pc);
        }
        CASE(GreaterThanInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 > i2);
            DISPATCH(++pc);
        }
        CASE(GreaterOrEqualInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 >= i2);
            DISPATCH(++pc);
        }
        CASE(LessThanInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 < i2);
            DISPATCH(++pc);
        }
        CASE(LessOrEqualInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 <= i2);
            DISPATCH(++pc);
        }
        CASE(AndInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].i = i1 & i2;
            DISPATCH(++pc);
        }
        CASE(OrInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].i = i1 | i2;
            DISPATCH(++pc);
        }
        CASE(XorInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].i = i1 ^ i2;
            DISPATCH(++pc);
        }
        CASE(ComplementInt): {
            int64_t i = stack[stack.len - 1].i;
            stack[stack.len - 1].i = ~i;
            DISPATCH(++pc);
        }
        CASE(LeftShiftInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            if (RG_UNLIKELY(i2 >= 64)) {
                stack[--stack.len - 1].i = 0;
            } else if (RG_LIKELY(i2 >= 0)) {
                stack[--stack.len - 1].i = (int64_t)((uint64_t)i1 << i2);
            } else {
                frame->pc = pc;
                FatalError("Left-shift by negative value is illegal");
                return false;
            }
            DISPATCH(++pc);
        }
        CASE(RightShiftInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            if (RG_UNLIKELY(i2 >= 64)) {
                stack[--stack.len - 1].i = 0;
            } else if (RG_LIKELY(i2 >= 0)) {
                stack[--stack.len - 1].i = (int64_t)((uint64_t)i1 >> i2);
            } else {
                frame->pc = pc;
                FatalError("Right-shift by negative value is illegal");
                return false;
            }
            DISPATCH(++pc);
        }
        CASE(LeftRotateInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i % 64;
            if (RG_UNLIKELY(i2 < 0)) {
                frame->pc = pc;
                FatalError("Left-rotate by negative value is illegal");
                return false;
            }
            stack[--stack.len - 1].i = (int64_t)(((uint64_t)i1 << i2) | ((uint64_t)i1 >> (64 - i2)));
            DISPATCH(++pc);
        }
        CASE(RightRotateInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i % 64;
            if (RG_UNLIKELY(i2 < 0)) {
                frame->pc = pc;
                FatalError("Right-rotate by negative value is illegal");
                return false;
            }
            stack[--stack.len - 1].i = (int64_t)(((uint64_t)i1 >> i2) | ((uint64_t)i1 << (64 - i2)));
            DISPATCH(++pc);
        }

        CASE(NegateFloat): {
            double d = stack[stack.len - 1].d;
            stack[stack.len - 1].d = -d;
            DISPATCH(++pc);
        }
        CASE(AddFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 + d2;
            DISPATCH(++pc);
        }
        CASE(SubstractFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 - d2;
            DISPATCH(++pc);
        }
        CASE(MultiplyFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 * d2;
            DISPATCH(++pc);
        }
        CASE(DivideFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 / d2;
            DISPATCH(++pc);
        }
        CASE(EqualFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 == d2);
            DISPATCH(++pc);
        }
        CASE(NotEqualFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 != d2);
            DISPATCH(++pc);
        }
        CASE(GreaterThanFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 > d2);
            DISPATCH(++pc);
        }
        CASE(GreaterOrEqualFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 >= d2);
            DISPATCH(++pc);
        }
        CASE(LessThanFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 < d2);
            DISPATCH(++pc);
        }
        CASE(LessOrEqualFloat): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 <= d2);
            DISPATCH(++pc);
        }

        CASE(EqualBool): {
            bool b1 = stack[stack.len - 2].b;
            bool b2 = stack[stack.len - 1].b;
            stack[--stack.len - 1].b = (b1 == b2);
            DISPATCH(++pc);
        }
        CASE(NotEqualBool): {
            bool b1 = stack[stack.len - 2].b;
            bool b2 = stack[stack.len - 1].b;
            stack[--stack.len - 1].b = (b1 != b2);
            DISPATCH(++pc);
        }
        CASE(NotBool): {
            bool b = stack[stack.len - 1].b;
            stack[stack.len - 1].b = !b;
            DISPATCH(++pc);
        }
        CASE(AndBool): {
            bool b1 = stack[stack.len - 2].b;
            bool b2 = stack[stack.len - 1].b;
            stack[--stack.len - 1].b = b1 && b2;
            DISPATCH(++pc);
        }
        CASE(OrBool): {
            bool b1 = stack[stack.len - 2].b;
            bool b2 = stack[stack.len - 1].b;
            stack[--stack.len - 1].b = b1 || b2;
            DISPATCH(++pc);
        }

        CASE(EqualString): {
            const char *str1 = stack[stack.len - 2].str;
            const char *str2 = stack[stack.len - 1].str;
            // Strings are interned so we only need to compare pointers
            stack[--stack.len - 1].b = (str1 == str2);
            DISPATCH(++pc);
        }
        CASE(NotEqualString): {
            const bk_TypeInfo *type1 = stack[stack.len - 2].type;
            const bk_TypeInfo *type2 = stack[stack.len - 1].type;
            stack[--stack.len - 1].b = (type1 != type2);
            DISPATCH(++pc);
        }

        CASE(EqualType): {
            const bk_TypeInfo *type1 = stack[stack.len - 2].type;
            const bk_TypeInfo *type2 = stack[stack.len - 1].type;
            stack[--stack.len - 1].b = (type1 == type2);
            DISPATCH(++pc);
        }
        CASE(NotEqualType): {
            const bk_TypeInfo *type1 = stack[stack.len - 2].type;
            const bk_TypeInfo *type2 = stack[stack.len - 1].type;
            stack[--stack.len - 1].b = (type1 != type2);
            DISPATCH(++pc);
        }

        CASE(EqualFunc): {
            const bk_FunctionInfo *func1 = stack[stack.len - 2].func;
            const bk_FunctionInfo *func2 = stack[stack.len - 1].func;
            stack[--stack.len - 1].b = (func1 == func2);
            DISPATCH(++pc);
        }
        CASE(NotEqualFunc): {
            const bk_FunctionInfo *func1 = stack[stack.len - 2].func;
            const bk_FunctionInfo *func2 = stack[stack.len - 1].func;
            stack[--stack.len - 1].b = (func1 != func2);
            DISPATCH(++pc);
        }

        CASE(Jump): {
            DISPATCH(pc += (Size)inst->u.i);
        }
        CASE(BranchIfTrue): {
            bool b = stack.ptr[--stack.len].b;
            DISPATCH(pc += (b ? (Size)inst->u.i : 1));
        }
        CASE(BranchIfFalse): {
            bool b = stack.ptr[--stack.len].b;
            DISPATCH(pc += (b ? 1 : (Size)inst->u.i));
        }
        CASE(SkipIfTrue): {
            bool b = stack[stack.len - 1].b;
            DISPATCH(pc += (b ? (Size)inst->u.i : 1));
        }
        CASE(SkipIfFalse): {
            bool b = stack[stack.len - 1].b;
            DISPATCH(pc += (b ? 1 : (Size)inst->u.i));
        }

        CASE(Call): {
            const bk_FunctionInfo *func = stack[stack.len + inst->u.i].func;

            if (func->mode == bk_FunctionInfo::Mode::Record) {
                // Nothing to do, the arguments build the object and that's it!
                // We just need to leave everything as-is on the stack

                DISPATCH(++pc);
            } else {
                // Save current PC
                frame->pc = pc;

                frame = frames.AppendDefault();
                frame->func = func;
                frame->pc = func->addr;
                frame->direct = false;

                if (func->mode == bk_FunctionInfo::Mode::Blikk) {
                    frame->bp = stack.len - func->params.len;

                    bp = frame->bp;
                    DISPATCH(pc = frame->pc);
                } else {
                    RG_ASSERT(func->mode == bk_FunctionInfo::Mode::Native);

                    if (func->type->variadic) {
                        Span<const bk_PrimitiveValue> args;
                        args.len = func->params.len + stack[stack.len - 1].i;
                        args.ptr = stack.end() - args.len - 1;

                        bk_PrimitiveValue ret = func->native(this, args);

                        stack.len -= args.len + 1;
                        stack[stack.len - 1] = ret;
                    } else {
                        Span<const bk_PrimitiveValue> args;
                        args.len = func->params.len;
                        args.ptr = stack.end() - args.len;

                        bk_PrimitiveValue ret = func->native(this, args);

                        stack.len -= args.len;
                        stack[stack.len - 1] = ret;
                    }

                    frames.RemoveLast(1);
                    frame = &frames[frames.len - 1];

                    if (RG_UNLIKELY(!run))
                        return !error;

                    DISPATCH(++pc);
                }
            }
        }
        CASE(CallDirect): {
            const bk_FunctionInfo *func = inst->u.func;

            // Save current PC
            frame->pc = pc;

            frame = frames.AppendDefault();
            frame->func = func;
            frame->pc = func->addr;
            frame->direct = true;

            if (func->mode == bk_FunctionInfo::Mode::Blikk) {
                frame->bp = stack.len - func->params.len;

                bp = frame->bp;
                DISPATCH(pc = frame->pc);
            } else {
                RG_ASSERT(func->mode == bk_FunctionInfo::Mode::Native);

                if (func->type->variadic) {
                    Span<const bk_PrimitiveValue> args;
                    args.len = func->params.len + stack[stack.len - 1].i;
                    args.ptr = stack.end() - args.len - 1;

                    bk_PrimitiveValue ret = func->native(this, args);

                    stack.len -= args.len;
                    stack[stack.len - 1] = ret;
                } else {
                    Span<const bk_PrimitiveValue> args;
                    args.len = func->params.len;
                    args.ptr = stack.end() - args.len;

                    bk_PrimitiveValue ret = func->native(this, args);

                    if (args.len) {
                        stack.len -= args.len - 1;
                    } else {
                        stack.len -= args.len;
                        stack.AppendDefault();
                    }
                    stack[stack.len - 1] = ret;
                }

                frames.RemoveLast(1);
                frame = &frames[frames.len - 1];

                if (RG_UNLIKELY(!run))
                    return !error;

                DISPATCH(++pc);
            }
        }
        CASE(Return): {
            stack[bp + frame->direct - 1] = stack[stack.len - 1];
            stack.len = bp + frame->direct;

            frames.RemoveLast(1);
            frame = &frames[frames.len - 1];
            pc = frame->pc;
            bp = frame->bp;

            DISPATCH(++pc);
        }

        CASE(IntToFloat): {
            int64_t i = stack[stack.len - 1].i;
            stack[stack.len - 1].d = (double)i;
            DISPATCH(++pc);
        }
        CASE(FloatToInt): {
            double d = stack[stack.len - 1].d;
            stack[stack.len - 1].i = (int64_t)d;
            DISPATCH(++pc);
        }

        CASE(End): {
            RG_ASSERT(stack.len == inst->u.i);

            pc++;
            return true;
        }
    }

    RG_UNREACHABLE();

#undef CASE
#undef LOOP
#undef DISPATCH
}

void bk_VirtualMachine::DumpInstruction(Size pc) const
{
    const bk_Instruction &inst = ir[pc];

    switch (inst.code) {
        case bk_Opcode::Push: {
            switch (inst.primitive) {
                case bk_PrimitiveKind::Null: { PrintLn(stderr, "%!D..[0x%1]%!0 Push null", FmtHex(pc).Pad0(-5)); } break;
                case bk_PrimitiveKind::Boolean: { PrintLn(stderr, "%!D..[0x%1]%!0 Push %2", FmtHex(pc).Pad0(-5), inst.u.b); } break;
                case bk_PrimitiveKind::Integer: { PrintLn(stderr, "%!D..[0x%1]%!0 Push %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
                case bk_PrimitiveKind::Float: { PrintLn(stderr, "%!D..[0x%1]%!0 Push %2", FmtHex(pc).Pad0(-5), FmtDouble(inst.u.d, 1, INT_MAX)); } break;
                case bk_PrimitiveKind::String: { PrintLn(stderr, "%!D..[0x%1]%!0 Push '%2'", FmtHex(pc).Pad0(-5), inst.u.str); } break;
                case bk_PrimitiveKind::Type: { PrintLn(stderr, "%!D..[0x%1]%!0 Push %2", FmtHex(pc).Pad0(-5), inst.u.type->signature); } break;
                case bk_PrimitiveKind::Function: { PrintLn(stderr, "%!D..[0x%1]%!0 Push %2", FmtHex(pc).Pad0(-5), inst.u.func->prototype); } break;
                case bk_PrimitiveKind::Array: { PrintLn(stderr, "%!D..[0x%1]%!0 Push @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
                case bk_PrimitiveKind::Record: { PrintLn(stderr, "%!D..[0x%1]%!0 Push @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            }
        } break;
        case bk_Opcode::Pop: { PrintLn(stderr, "%!D..[0x%1]%!0 Pop %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case bk_Opcode::Load: { PrintLn(stderr, "%!D..[0x%1]%!0 Load @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::LoadLocal: { PrintLn(stderr, "%!D..[0x%1]%!0 LoadLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::Lea: { PrintLn(stderr, "%!D..[0x%1]%!0 Lea @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::LeaLocal: { PrintLn(stderr, "%!D..[0x%1]%!0 LeaLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::Store: { PrintLn(stderr, "%!D..[0x%1]%!0 Store @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::StoreLocal: { PrintLn(stderr, "%!D..[0x%1]%!0 StoreLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::Copy: { PrintLn(stderr, "%!D..[0x%1]%!0 Copy @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::CopyLocal: { PrintLn(stderr, "%!D..[0x%1]%!0 CopyLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::CheckIndex: { PrintLn(stderr, "%!D..[0x%1]%!0 CheckIndex (%2)", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case bk_Opcode::Jump: { PrintLn(stderr, "%!D..[0x%1]%!0 Jump 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::BranchIfTrue: { PrintLn(stderr, "%!D..[0x%1]%!0 BranchIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::BranchIfFalse: { PrintLn(stderr, "%!D..[0x%1]%!0 BranchIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::SkipIfTrue: { PrintLn(stderr, "%!D..[0x%1]%!0 SkipIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::SkipIfFalse: { PrintLn(stderr, "%!D..[0x%1]%!0 SkipIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;

        case bk_Opcode::Call: { PrintLn(stderr, "%!D..[0x%1]%!0 Call %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case bk_Opcode::CallDirect: {
            const bk_FunctionInfo *func = inst.u.func;
            PrintLn(stderr, "%!D..[0x%1]%!0 CallDirect %2", FmtHex(pc).Pad0(-5), func->prototype);
        } break;

        default: { PrintLn(stderr, "%!D..[0x%1]%!0 %2", FmtHex(pc).Pad0(-5), bk_OpcodeNames[(int)inst.code]); } break;
    }
}

bool bk_Run(const bk_Program &program, bool debug)
{
    bk_VirtualMachine vm(&program);
    return vm.Run(debug);
}

}
