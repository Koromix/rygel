// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "program.hh"
#include "vm.hh"

namespace K {

bk_VirtualMachine::bk_VirtualMachine(const bk_Program *const program, unsigned int flags)
    : flags(flags), program(program)
{
    frames.AppendDefault();
}

bool bk_VirtualMachine::Run()
{
    bool debug = flags & (int)bk_RunFlag::Debug;

    run = true;
    error = false;

    bk_CallFrame *frame = &frames[frames.len - 1];
    Size pc = frame->pc;
    Size bp = frame->bp;
    Span<const bk_Instruction> ir = frame->func ? frame->func->ir : program->main;
    K_ASSERT(pc < ir.len);

    // Save PC on exit
    K_DEFER { frame->pc = pc; };

    const bk_Instruction *inst;

#if defined(__GNUC__) || defined(__clang__)
    static const void *dispatch[] = {
        #define OPCODE(Code) && Code,
        #include "opcodes.inc"
    };

    #define DISPATCH(PC) \
        inst = &ir[(PC)]; \
        if (debug) { \
            DumpInstruction(*inst, pc, bp); \
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
            DumpInstruction(*inst, pc, bp); \
        } \
        break
    #define LOOP \
        inst = &ir[pc]; \
        if (debug) { \
            DumpInstruction(*inst, pc, bp); \
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
            stack.Append({ .i = inst->u2.i });
            DISPATCH(++pc);
        }
        CASE(Reserve): {
            stack.AppendDefault(inst->u2.i);
            DISPATCH(++pc);
        }
        CASE(Fetch): {
            Span<bk_PrimitiveValue> data = program->ro.Take(inst->u2.i, inst->u1.i);
            stack.Append(data);
            DISPATCH(++pc);
        }
        CASE(Pop): {
            stack.RemoveLast(inst->u2.i);
            DISPATCH(++pc);
        }

        CASE(Lea): {
            stack.Append({ .i = inst->u2.i });
            DISPATCH(++pc);
        }
        CASE(LeaLocal): {
            stack.Append({ .i = bp + inst->u2.i });
            DISPATCH(++pc);
        }
        CASE(LeaRel): {
            stack.Append({ .i = stack.len + inst->u2.i });
            DISPATCH(++pc);
        }
        CASE(Load): {
            stack.Append({ .i = stack[inst->u2.i].i });
            DISPATCH(++pc);
        }
        CASE(LoadLocal): {
            stack.Append({ .i = stack[bp + inst->u2.i].i });
            DISPATCH(++pc);
        }
        CASE(LoadIndirect): {
            Size ptr = stack.ptr[--stack.len].i;
            for (Size i = 0; i < inst->u2.i; i++) {
                stack.Append({ .i = stack[ptr + i].i });
            }
            DISPATCH(++pc);
        }
        CASE(LoadIndirectK): {
            Size ptr = stack[stack.len - 1].i;
            for (Size i = 0; i < inst->u2.i; i++) {
                stack.Append({ .i = stack[ptr + i].i });
            }
            DISPATCH(++pc);
        }
        CASE(Store): {
            stack[inst->u2.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreK): {
            stack[inst->u2.i].i = stack[stack.len - 1].i;
            DISPATCH(++pc);
        }
        CASE(StoreLocal): {
            stack[bp + inst->u2.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreLocalK): {
            stack[bp + inst->u2.i].i = stack.ptr[stack.len - 1].i;
            DISPATCH(++pc);
        }
        CASE(StoreIndirect): {
            Size ptr = stack[stack.len - inst->u2.i - 1].i;
            Size src = stack.len - inst->u2.i;
            for (Size i = inst->u2.i - 1; i >= 0; i--) {
                stack[ptr + i].i = stack[src + i].i;
            }
            stack.len -= inst->u2.i + 1;
            DISPATCH(++pc);
        }
        CASE(StoreIndirectK): {
            Size ptr = stack[stack.len - inst->u2.i - 1].i;
            Size src = stack.len - inst->u2.i;
            for (Size i = inst->u2.i - 1; i >= 0; i--) {
                int64_t value = stack[src + i].i;
                stack[ptr + i].i = value;
                stack[src + i - 1].i = value;
            }
            stack.len--;
            DISPATCH(++pc);
        }
        CASE(StoreRev): {
            Size ptr = stack.ptr[--stack.len].i;
            Size src = stack.len - inst->u2.i;
            for (Size i = inst->u2.i - 1; i >= 0; i--) {
                stack[ptr + i].i = stack[src + i].i;
            }
            stack.len -= inst->u2.i;
            DISPATCH(++pc);
        }
        CASE(StoreRevK): {
            Size ptr = stack.ptr[--stack.len].i;
            Size src = stack.len - inst->u2.i;
            for (Size i = inst->u2.i - 1; i >= 0; i--) {
                stack[ptr + i].i = stack[src + i].i;
            }
            DISPATCH(++pc);
        }
        CASE(CheckIndex): {
            Size idx = stack[stack.len - 1].i;
            if (idx < 0 || idx >= inst->u2.i) [[unlikely]] {
                frame->pc = pc;
                FatalError("Index is out of range: %1 (array length %2)", idx, inst->u2.i);
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
            if (i2 == 0) [[unlikely]] {
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
            if (i2 == 0) [[unlikely]] {
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
            if (i2 >= 64) [[unlikely]] {
                stack[--stack.len - 1].i = 0;
            } else if (i2 >= 0) [[likely]] {
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
            if (i2 >= 64) [[unlikely]] {
                stack[--stack.len - 1].i = 0;
            } else if (i2 >= 0) [[likely]] {
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
            if (i2 < 0) [[unlikely]] {
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
            if (i2 < 0) [[unlikely]] {
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
            const char *str1 = stack[stack.len - 2].str;
            const char *str2 = stack[stack.len - 1].str;
            // Strings are interned so we only need to compare pointers
            stack[--stack.len - 1].b = (str1 != str2);
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

        CASE(EqualEnum): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 == i2);
            DISPATCH(++pc);
        }
        CASE(NotEqualEnum): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].b = (i1 != i2);
            DISPATCH(++pc);
        }

        CASE(Jump): {
            DISPATCH(pc += (Size)inst->u2.i);
        }
        CASE(BranchIfTrue): {
            bool b = stack.ptr[--stack.len].b;
            DISPATCH(pc += (b ? (Size)inst->u2.i : 1));
        }
        CASE(BranchIfFalse): {
            bool b = stack.ptr[--stack.len].b;
            DISPATCH(pc += (b ? 1 : (Size)inst->u2.i));
        }
        CASE(SkipIfTrue): {
            bool b = stack[stack.len - 1].b;
            DISPATCH(pc += (b ? (Size)inst->u2.i : 1));
        }
        CASE(SkipIfFalse): {
            bool b = stack[stack.len - 1].b;
            DISPATCH(pc += (b ? 1 : (Size)inst->u2.i));
        }

        CASE(CallIndirect): {
            const bk_FunctionInfo *func = stack[stack.len + inst->u2.i].func;
            const bk_FunctionTypeInfo *func_type = func->type;
            const bk_TypeInfo *ret_type = func_type->ret_type;

            if (!func->valid) [[unlikely]] {
                frame->pc = pc;
                FatalError("Calling invalid function '%1'", func->prototype);
                return false;
            }

            if (func->mode == bk_FunctionInfo::Mode::Record) {
                // Nothing to do, the arguments build the object and that's it!
                // We just need to leave everything as-is on the stack

                DISPATCH(++pc);
            } else {
                // Save current PC
                frame->pc = pc;

                frame = frames.AppendDefault();
                frame->func = func;
                frame->direct = false;

                if (func->mode == bk_FunctionInfo::Mode::Blikk) {
                    frame->bp = stack.len - func->type->params_size;

                    bp = frame->bp;
                    ir = func->ir;

                    DISPATCH(pc = 1); // Skip NOP
                } else {
                    K_ASSERT(func->mode == bk_FunctionInfo::Mode::Native);

                    stack.Grow(ret_type->size);

                    if (func_type->variadic) {
                        Size variadic_arg = stack[stack.len - 1].i;
                        Span<bk_PrimitiveValue> args = MakeSpan(stack.end() - func_type->params_size - variadic_arg - 1,
                                                                func_type->params_size + variadic_arg);
                        Span<bk_PrimitiveValue> ret = MakeSpan(stack.end(), ret_type->size);
                        stack.len += ret.len;

                        func->native(this, args, ret);

                        MemMove(args.ptr - 1, ret.ptr, ret.len * K_SIZE(*ret.ptr));
                        stack.len -= args.len + 2;
                    } else {
                        Span<bk_PrimitiveValue> args = MakeSpan(stack.end() - func_type->params_size, func_type->params_size);
                        Span<bk_PrimitiveValue> ret = MakeSpan(stack.end(), ret_type->size);
                        stack.len += ret.len;

                        func->native(this, args, ret);

                        MemMove(args.ptr - 1, ret.ptr, ret.len * K_SIZE(*ret.ptr));
                        stack.len -= args.len + 1;
                    }

                    frames.RemoveLast(1);
                    frame = &frames[frames.len - 1];

                    if (!run) [[unlikely]]
                        return !error;

                    DISPATCH(++pc);
                }
            }
        }
        CASE(Call): {
            const bk_FunctionInfo *func = inst->u2.func;
            const bk_FunctionTypeInfo *func_type = func->type;
            const bk_TypeInfo *ret_type = func_type->ret_type;

            if (!func->valid) [[unlikely]] {
                frame->pc = pc;
                FatalError("Calling invalid function '%1'", func->prototype);
                return false;
            }

            // Save current PC
            frame->pc = pc;

            frame = frames.AppendDefault();
            frame->func = func;
            frame->direct = true;

            if (func->mode == bk_FunctionInfo::Mode::Blikk) {
                frame->bp = stack.len - func->type->params_size;

                bp = frame->bp;
                ir = func->ir;

                DISPATCH(pc = 1); // Skip NOP
            } else {
                K_ASSERT(func->mode == bk_FunctionInfo::Mode::Native);

                stack.Grow(ret_type->size);

                if (func_type->variadic) {
                    Size variadic_arg = stack[stack.len - 1].i;
                    Span<bk_PrimitiveValue> args = MakeSpan(stack.end() - func_type->params_size - variadic_arg - 1,
                                                            func_type->params_size + variadic_arg);
                    Span<bk_PrimitiveValue> ret = MakeSpan(stack.end(), ret_type->size);
                    stack.len += ret.len;

                    func->native(this, args, ret);

                    MemMove(args.ptr, ret.ptr, ret.len * K_SIZE(*ret.ptr));
                    stack.len -= args.len + 1;
                } else {
                    Span<bk_PrimitiveValue> args = MakeSpan(stack.end() - func_type->params_size, func_type->params_size);
                    Span<bk_PrimitiveValue> ret = MakeSpan(stack.end(), ret_type->size);
                    stack.len += ret.len;

                    func->native(this, args, ret);

                    MemMove(args.ptr, ret.ptr, ret.len * K_SIZE(*ret.ptr));
                    stack.len -= args.len;
                }

                frames.RemoveLast(1);
                frame = &frames[frames.len - 1];

                if (!run) [[unlikely]]
                    return !error;

                DISPATCH(++pc);
            }
        }
        CASE(Return): {
            Size src = stack.len - inst->u2.i;
            stack.len = bp - 1 + frame->direct;
            stack.Grow(inst->u2.i);
            for (Size i = 0; i < inst->u2.i; i++) {
                stack[stack.len++].i = stack.ptr[src + i].i;
            }

            frames.RemoveLast(1);
            frame = &frames[frames.len - 1];
            pc = frame->pc;
            bp = frame->bp;
            ir = frame->func ? frame->func->ir : program->main;

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
        CASE(InlineIf): {
            Size ptr = stack.len - 2 * inst->u2.i - 1;
            Size src = stack[ptr].b ? (ptr + 1) : (ptr + 1 + inst->u2.i);
            MemCpy(stack.ptr + ptr, stack.ptr + src, inst->u2.i * K_SIZE(*stack.ptr));
            stack.len = ptr + inst->u2.i;
            DISPATCH(++pc);
        }

        CASE(End): {
            K_ASSERT(stack.len == inst->u2.i);

            pc++;
            return true;
        }
    }

    K_UNREACHABLE();

#undef CASE
#undef LOOP
#undef DISPATCH
}

void bk_VirtualMachine::DumpInstruction(const bk_Instruction &inst, Size pc, Size bp) const
{
    Print(StdErr, "%!D..[0x%1]%!0 %2%!..+%3%!0",
          FmtHex(pc, 6), FmtRepeat("  ", (int)frames.len - 1), bk_OpcodeNames[(int)inst.code]);

    switch (inst.code) {
        case bk_Opcode::Push: {
            switch (inst.u1.primitive) {
                case bk_PrimitiveKind::Null: { K_UNREACHABLE(); } break;
                case bk_PrimitiveKind::Boolean: { PrintLn(StdErr, " %!Y..[Bool]%!0 %1 %!M..>%2%!0", inst.u2.b, stack.len); } break;
                case bk_PrimitiveKind::Integer: { PrintLn(StdErr, " %!Y..[Int]%!0 %1 %!M..>%2%!0", inst.u2.i, stack.len); } break;
                case bk_PrimitiveKind::Float: { PrintLn(StdErr, " %!Y..[Float]%!0 %1 %!M..>%2%!0", FmtDouble(inst.u2.d, 1, INT_MAX), stack.len); } break;
                case bk_PrimitiveKind::String: { PrintLn(StdErr, " %!Y..[String]%!0 '%1' %!M..>%2%!0", inst.u2.str ? inst.u2.str : "", stack.len); } break;
                case bk_PrimitiveKind::Type: { PrintLn(StdErr, " %!Y..[Type]%!0 '%1' %!M..>%2%!0", inst.u2.type->signature, stack.len); } break;
                case bk_PrimitiveKind::Function: { PrintLn(StdErr, " %!Y..[Function]%!0 '%1' %!M..>%2%!0", inst.u2.func->prototype, stack.len); } break;
                case bk_PrimitiveKind::Array: { PrintLn(StdErr, " %!Y..[Array]%!0 %!M..>%1%!0", stack.len); } break;
                case bk_PrimitiveKind::Record: { PrintLn(StdErr, " %!Y..[Record]%!0 %!M..>%1%!0", stack.len); } break;
                case bk_PrimitiveKind::Enum: { PrintLn(StdErr, " %!Y..[Enum]%!0 %1 %!M..>%2%!0", inst.u2.i, stack.len); } break;
                case bk_PrimitiveKind::Opaque: { PrintLn(StdErr, " %!Y..[Opaque]%!0 0x%1 %!M..>%2%!0", FmtHex((uintptr_t)inst.u2.opaque, K_SIZE(void *) * 2), stack.len); } break;
            }
        } break;
        case bk_Opcode::Reserve: { PrintLn(StdErr, " |%1 %!M..>%2%!0", inst.u2.i, stack.len); } break;
        case bk_Opcode::Fetch: { PrintLn(StdErr, " %!R..<%1%!0 |%2 %!M..>%3%!0", inst.u2.i, inst.u1.i, stack.len); } break;
        case bk_Opcode::Pop: { PrintLn(StdErr, " %1", inst.u2.i); } break;

        case bk_Opcode::Lea: { PrintLn(StdErr, " %!R..@%1%!0 %!M..>%2%!0", inst.u2.i, stack.len); } break;
        case bk_Opcode::LeaLocal: { PrintLn(StdErr, " %!R..@%1%!0 %!M..>%2%!0", bp + inst.u2.i, stack.len); } break;
        case bk_Opcode::LeaRel: { PrintLn(StdErr, " %!R..@%1%!0 %!M..>%2%!0", stack.len + inst.u2.i, stack.len); } break;
        case bk_Opcode::Load: { PrintLn(StdErr, " %!R..@%1%!0 %!M..>%2%!0", inst.u2.i, stack.len); } break;
        case bk_Opcode::LoadLocal: { PrintLn(StdErr, " %!R..@%1%!0 %!M..>%2%!0", bp + inst.u2.i, stack.len); } break;
        case bk_Opcode::LoadIndirect: { PrintLn(StdErr, " |%1 %!M..>%2%!0", inst.u2.i, stack.len - 1); } break;
        case bk_Opcode::LoadIndirectK: { PrintLn(StdErr, " |%1 %!M..>%2%!0", inst.u2.i, stack.len); } break;
        case bk_Opcode::Store:
        case bk_Opcode::StoreK: { PrintLn(StdErr, " %!M..>%1%!0", inst.u2.i); } break;
        case bk_Opcode::StoreLocal:
        case bk_Opcode::StoreLocalK: { PrintLn(StdErr, " %!M..>%1%!0", bp + inst.u2.i); } break;
        case bk_Opcode::StoreIndirect:
        case bk_Opcode::StoreIndirectK:
        case bk_Opcode::StoreRev:
        case bk_Opcode::StoreRevK: { PrintLn(StdErr, " |%1", inst.u2.i); } break;
        case bk_Opcode::CheckIndex: { PrintLn(StdErr, " < %1", inst.u2.i); } break;

        case bk_Opcode::Jump:
        case bk_Opcode::BranchIfTrue:
        case bk_Opcode::BranchIfFalse:
        case bk_Opcode::SkipIfTrue:
        case bk_Opcode::SkipIfFalse: { PrintLn(StdErr, " %!G..0x%1%!0", FmtHex(pc + inst.u2.i, 6)); } break;

        case bk_Opcode::CallIndirect: { PrintLn(StdErr, " %!R..@%1%!0", stack.len + inst.u2.i); } break;
        case bk_Opcode::Call: {
            const bk_FunctionInfo *func = inst.u2.func;
            PrintLn(StdErr, " %!G..'%1'%!0", func->prototype);
        } break;
        case bk_Opcode::Return: { PrintLn(StdErr, " %1", inst.u2.i); } break;

        case bk_Opcode::InlineIf: { PrintLn(StdErr, " |%1 %!M..>%2%!0", inst.u2.i, stack.len - 2 * inst.u2.i - 1); } break;

        case bk_Opcode::End: { PrintLn(StdErr, " (%1)", inst.u2.i); } break;

        default: { PrintLn(StdErr); } break;
    }
}

bool bk_Run(const bk_Program &program, unsigned int flags)
{
    bk_VirtualMachine vm(&program, flags);
    return vm.Run();
}

}
