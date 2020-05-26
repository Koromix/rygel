// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "program.hh"
#include "vm.hh"

namespace RG {

VirtualMachine::VirtualMachine(const Program *const program)
    : program(program)
{
    frames.AppendDefault();
}

bool VirtualMachine::Run()
{
    const Instruction *inst;

    ir = program->ir;
    run = true;
    error = false;

    CallFrame *frame = &frames[frames.len - 1];
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
        case Opcode::Code
#endif

    LOOP {
        CASE(PushNull): {
            stack.AppendDefault();
            DISPATCH(++pc);
        }
        CASE(PushBool): {
            stack.Append({.b = inst->u.b});
            DISPATCH(++pc);
        }
        CASE(PushInt): {
            stack.Append({.i = inst->u.i});
            DISPATCH(++pc);
        }
        CASE(PushFloat): {
            stack.Append({.d = inst->u.d});
            DISPATCH(++pc);
        }
        CASE(PushString): {
            stack.Append({.str = inst->u.str});
            DISPATCH(++pc);
        }
        CASE(PushType): {
            stack.Append({.type = inst->u.type});
            DISPATCH(++pc);
        }
        CASE(Pop): {
            stack.RemoveLast(inst->u.i);
            DISPATCH(++pc);
        }

        CASE(LoadBool): {
            stack.Append({.b = stack[inst->u.i].b});
            DISPATCH(++pc);
        }
        CASE(LoadInt): {
            stack.Append({.i = stack[inst->u.i].i});
            DISPATCH(++pc);
        }
        CASE(LoadFloat): {
            stack.Append({.d = stack[inst->u.i].d});
            DISPATCH(++pc);
        }
        CASE(LoadString): {
            stack.Append({.str = stack[inst->u.i].str});
            DISPATCH(++pc);
        }
        CASE(LoadType): {
            stack.Append({.type = stack[inst->u.i].type});
            DISPATCH(++pc);
        }
        CASE(StoreBool): {
            stack[inst->u.i].b = stack.ptr[--stack.len].b;
            DISPATCH(++pc);
        }
        CASE(StoreInt): {
            stack[inst->u.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreFloat): {
            stack[inst->u.i].d = stack.ptr[--stack.len].d;
            DISPATCH(++pc);
        }
        CASE(StoreString): {
            stack[inst->u.i].str = stack.ptr[--stack.len].str;
            DISPATCH(++pc);
        }
        CASE(StoreType): {
            stack[inst->u.i].type = stack.ptr[--stack.len].type;
            DISPATCH(++pc);
        }
        CASE(CopyBool): {
            stack[inst->u.i].b = stack[stack.len - 1].b;
            DISPATCH(++pc);
        }
        CASE(CopyInt): {
            stack[inst->u.i].i = stack[stack.len - 1].i;
            DISPATCH(++pc);
        }
        CASE(CopyFloat): {
            stack[inst->u.i].d = stack[stack.len - 1].d;
            DISPATCH(++pc);
        }
        CASE(CopyString): {
            stack[inst->u.i].str = stack[stack.len - 1].str;
            DISPATCH(++pc);
        }
        CASE(CopyType): {
            stack[inst->u.i].type = stack[stack.len - 1].type;
            DISPATCH(++pc);
        }

        CASE(LoadLocalBool): {
            stack.Append({.b = stack[bp + inst->u.i].b});
            DISPATCH(++pc);
        }
        CASE(LoadLocalInt): {
            stack.Append({.i = stack[bp + inst->u.i].i});
            DISPATCH(++pc);
        }
        CASE(LoadLocalFloat): {
            stack.Append({.d = stack[bp + inst->u.i].d});
            DISPATCH(++pc);
        }
        CASE(LoadLocalString): {
            stack.Append({.str = stack[bp + inst->u.i].str});
            DISPATCH(++pc);
        }
        CASE(LoadLocalType): {
            stack.Append({.type = stack[bp + inst->u.i].type});
            DISPATCH(++pc);
        }
        CASE(StoreLocalBool): {
            stack[bp + inst->u.i].b = stack.ptr[--stack.len].b;
            DISPATCH(++pc);
        }
        CASE(StoreLocalInt): {
            stack[bp + inst->u.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreLocalFloat): {
            stack[bp + inst->u.i].d = stack.ptr[--stack.len].d;
            DISPATCH(++pc);
        }
        CASE(StoreLocalString): {
            stack[bp + inst->u.i].str = stack.ptr[--stack.len].str;
            DISPATCH(++pc);
        }
        CASE(StoreLocalType): {
            stack[bp + inst->u.i].type = stack.ptr[--stack.len].type;
            DISPATCH(++pc);
        }
        CASE(CopyLocalBool): {
            stack[bp + inst->u.i].b = stack.ptr[stack.len - 1].b;
            DISPATCH(++pc);
        }
        CASE(CopyLocalInt): {
            stack[bp + inst->u.i].i = stack.ptr[stack.len - 1].i;
            DISPATCH(++pc);
        }
        CASE(CopyLocalFloat): {
            stack[bp + inst->u.i].d = stack.ptr[stack.len - 1].d;
            DISPATCH(++pc);
        }
        CASE(CopyLocalString): {
            stack[bp + inst->u.i].str = stack.ptr[stack.len - 1].str;
            DISPATCH(++pc);
        }
        CASE(CopyLocalType): {
            stack[bp + inst->u.i].type = stack.ptr[stack.len - 1].type;
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
                FatalError("Right-shift by negative value is illegal");
                return false;
            }
            DISPATCH(++pc);
        }
        CASE(LeftRotateInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i % 64;
            if (RG_UNLIKELY(i2 < 0)) {
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

        CASE(EqualType): {
            const TypeInfo *type1 = stack[stack.len - 2].type;
            const TypeInfo *type2 = stack[stack.len - 1].type;
            stack[--stack.len - 1].b = (type1 == type2);
            DISPATCH(++pc);
        }
        CASE(NotEqualType): {
            const TypeInfo *type1 = stack[stack.len - 2].type;
            const TypeInfo *type2 = stack[stack.len - 1].type;
            stack[--stack.len - 1].b = (type1 != type2);
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
            const FunctionInfo *func = inst->u.func;
            RG_ASSERT(func->mode == FunctionInfo::Mode::Blik);

            // Save current PC
            frame->pc = pc;

            frame = frames.AppendDefault();
            frame->func = func;
            frame->pc = func->addr;
            frame->bp = stack.len - func->params.len;

            bp = frame->bp;
            DISPATCH(pc = frame->pc);
        }
        CASE(CallNative): {
            const FunctionInfo *func = inst->u.func;
            RG_ASSERT(func->mode == FunctionInfo::Mode::Native);

            // Save current PC
            frame->pc = pc;

            frame = frames.AppendDefault();
            frame->func = func;
            frame->pc = func->addr;

            if (func->variadic) {
                Span<const Value> args;
                args.len = func->params.len + (stack[stack.len - 1].i * 2);
                args.ptr = stack.end() - args.len - 1;

                stack.len -= args.len;
                stack[stack.len - 1] = func->native(this, args);
            } else {
                Span<const Value> args;
                args.len = func->params.len;
                args.ptr = stack.end() - args.len;

                if (args.len) {
                    stack.len -= args.len - 1;
                } else {
                    stack.len -= args.len;
                    stack.AppendDefault();
                }
                stack[stack.len - 1] = func->native(this, args);
            }

            frames.RemoveLast(1);
            frame = &frames[frames.len - 1];

            if (RG_UNLIKELY(!run))
                return !error;

            DISPATCH(++pc);
        }
        CASE(Return): {
            stack[bp] = stack[stack.len - 1];
            stack.len = bp + 1;

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

void VirtualMachine::DumpInstruction(Size pc) const
{
#if 0
    const Instruction &inst = ir[pc];

    switch (inst.code) {
        case Opcode::PushBool: { LogDebug("[0x%1] PushBool %2", FmtHex(pc).Pad0(-5), inst.u.b); } break;
        case Opcode::PushInt: { LogDebug("[0x%1] PushInt %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::PushFloat: { LogDebug("[0x%1] PushFloat %2", FmtHex(pc).Pad0(-5), inst.u.d); } break;
        case Opcode::PushString: { LogDebug("[0x%1] PushString %2", FmtHex(pc).Pad0(-5), inst.u.str); } break;
        case Opcode::PushType: { LogDebug("[0x%1] PushType %2", FmtHex(pc).Pad0(-5), inst.u.type->signature); } break;
        case Opcode::Pop: { LogDebug("[0x%1] Pop %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::LoadBool: { LogDebug("[0x%1] LoadBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadInt: { LogDebug("[0x%1] LoadInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadFloat: { LogDebug("[0x%1] LoadFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadString: { LogDebug("[0x%1] LoadString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadType: { LogDebug("[0x%1] LoadType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreBool: { LogDebug("[0x%1] StoreBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreInt: { LogDebug("[0x%1] StoreInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreFloat: { LogDebug("[0x%1] StoreFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreString: { LogDebug("[0x%1] StoreString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreType: { LogDebug("[0x%1] StoreType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyBool: { LogDebug("[0x%1] CopyBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyInt: { LogDebug("[0x%1] CopyInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyFloat: { LogDebug("[0x%1] CopyFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyString: { LogDebug("[0x%1] CopyString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyType: { LogDebug("[0x%1] CopyType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::LoadLocalBool: { LogDebug("[0x%1] LoadLocalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalInt: { LogDebug("[0x%1] LoadLocalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalFloat: { LogDebug("[0x%1] LoadLocalFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalString: { LogDebug("[0x%1] LoadLocalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalType: { LogDebug("[0x%1] LoadLocalType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalBool: { LogDebug("[0x%1] StoreLocalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalInt: { LogDebug("[0x%1] StoreLocalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalFloat: { LogDebug("[0x%1] StoreLocalFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalString: { LogDebug("[0x%1] StoreLocalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalType: { LogDebug("[0x%1] StoreLocalType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyLocalBool: { LogDebug("[0x%1] CopyLocalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyLocalInt: { LogDebug("[0x%1] CopyLocalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyLocalFloat: { LogDebug("[0x%1] CopyLocalFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyLocalString: { LogDebug("[0x%1] CopyLocalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyLocalType: { LogDebug("[0x%1] CopyLocalType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::Jump: { LogDebug("[0x%1] Jump 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::BranchIfTrue: { LogDebug("[0x%1] BranchIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::BranchIfFalse: { LogDebug("[0x%1] BranchIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::SkipIfTrue: { LogDebug("[0x%1] SkipIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::SkipIfFalse: { LogDebug("[0x%1] SkipIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;

        case Opcode::Call:
        case Opcode::CallNative: {
            const FunctionInfo *func = inst.u.func;
            LogDebug("[0x%1] %2 %3 (%4%5)", FmtHex(pc).Pad0(-5), OpcodeNames[(int)inst.code],
                                            func->name, func->params.len, func->variadic ? "+" : "");
        } break;

        default: { LogDebug("[0x%1] %2", FmtHex(pc).Pad0(-5), OpcodeNames[(int)inst.code]); } break;
    }
#endif
}

bool Run(const Program &program)
{
    VirtualMachine vm(&program);
    return vm.Run();
}

}
