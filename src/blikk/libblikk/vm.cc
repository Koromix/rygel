// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "program.hh"
#include "vm.hh"

namespace RG {

bk_VirtualMachine::bk_VirtualMachine(const bk_Program *const program)
    : program(program)
{
    frames.AppendDefault();
}

bool bk_VirtualMachine::Run()
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
        DumpInstruction(pc); \
        goto *dispatch[(int)inst->code];
    #define LOOP \
        DISPATCH(pc)
    #define CASE(Code) \
        Code
#else
    #define DISPATCH(PC) \
        inst = &ir[(PC)]; \
        DumpInstruction(pc); \
        break
    #define LOOP \
        inst = &ir[pc]; \
        DumpInstruction(pc); \
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
        CASE(LoadArray): {
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

            // Save current PC
            frame->pc = pc;

            frame = frames.AppendDefault();
            frame->func = func;
            frame->pc = func->addr;
            frame->direct = false;

            if (func->mode == bk_FunctionInfo::Mode::blikk) {
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
        CASE(CallDirect): {
            const bk_FunctionInfo *func = inst->u.func;

            // Save current PC
            frame->pc = pc;

            frame = frames.AppendDefault();
            frame->func = func;
            frame->pc = func->addr;
            frame->direct = true;

            if (func->mode == bk_FunctionInfo::Mode::blikk) {
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
#if 0
    const bk_Instruction &inst = ir[pc];

    switch (inst.code) {
        case bk_Opcode::Push: {
            switch (inst.primitive) {
                case bk_PrimitiveKind::Null: { LogDebug("[0x%1] Push null", FmtHex(pc).Pad0(-5)); } break;
                case bk_PrimitiveKind::Boolean: { LogDebug("[0x%1] Push %2", FmtHex(pc).Pad0(-5), inst.u.b); } break;
                case bk_PrimitiveKind::Integer: { LogDebug("[0x%1] Push %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
                case bk_PrimitiveKind::Float: { LogDebug("[0x%1] Push %2", FmtHex(pc).Pad0(-5), FmtDouble(inst.u.d, 1, INT_MAX)); } break;
                case bk_PrimitiveKind::String: { LogDebug("[0x%1] Push '%2'", FmtHex(pc).Pad0(-5), inst.u.str); } break;
                case bk_PrimitiveKind::Type: { LogDebug("[0x%1] Push %2", FmtHex(pc).Pad0(-5), inst.u.type->signature); } break;
                case bk_PrimitiveKind::Function: { LogDebug("[0x%1] Push %2", FmtHex(pc).Pad0(-5), inst.u.func->prototype); } break;
                case bk_PrimitiveKind::Array: { LogDebug("[0x%1] Push @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
            }
        } break;
        case bk_Opcode::Pop: { LogDebug("[0x%1] Pop %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case bk_Opcode::Load: { LogDebug("[0x%1] Load @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::LoadLocal: { LogDebug("[0x%1] LoadLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::Lea: { LogDebug("[0x%1] Lea @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::LeaLocal: { LogDebug("[0x%1] LeaLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::Store: { LogDebug("[0x%1] Store @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::StoreLocal: { LogDebug("[0x%1] StoreLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::Copy: { LogDebug("[0x%1] Copy @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::CopyLocal: { LogDebug("[0x%1] CopyLocal @%2 (%3)", FmtHex(pc).Pad0(-5), inst.u.i, bk_PrimitiveKindNames[(int)inst.primitive]); } break;
        case bk_Opcode::CheckIndex: { LogDebug("[0x%1] CheckIndex (%2)", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case bk_Opcode::Jump: { LogDebug("[0x%1] Jump 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::BranchIfTrue: { LogDebug("[0x%1] BranchIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::BranchIfFalse: { LogDebug("[0x%1] BranchIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::SkipIfTrue: { LogDebug("[0x%1] SkipIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case bk_Opcode::SkipIfFalse: { LogDebug("[0x%1] SkipIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;

        case bk_Opcode::Call: { LogDebug("[0x%1] %2 %3", FmtHex(pc).Pad0(-5), bk_OpcodeNames[(int)inst.code], inst.u.i); } break;
        case bk_Opcode::CallDirect: {
            const bk_FunctionInfo *func = inst.u.func;
            LogDebug("[0x%1] %2 %3", FmtHex(pc).Pad0(-5), bk_OpcodeNames[(int)inst.code], func->prototype);
        } break;

        default: { LogDebug("[0x%1] %2", FmtHex(pc).Pad0(-5), bk_OpcodeNames[(int)inst.code]); } break;
    }
#endif
}

bool bk_Run(const bk_Program &program)
{
    bk_VirtualMachine vm(&program);
    return vm.Run();
}

}
