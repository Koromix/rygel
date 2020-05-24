// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "program.hh"
#include "vm.hh"

namespace RG {

bool VirtualMachine::Run()
{
    const Instruction *inst;

    ir = program->ir;
    run = true;
    error = false;

    RG_ASSERT(pc < ir.len);

#if defined(__GNUC__) || defined(__clang__)
    static const void *dispatch[] = {
        #define OPCODE(Code) && Code,
        #include "opcodes.inc"
    };

    #define DISPATCH(PC) \
        inst = &ir[(PC)]; \
        DumpInstruction(); \
        goto *dispatch[(int)inst->code];
    #define LOOP \
        DISPATCH(pc)
    #define CASE(Code) \
        Code
#else
    #define DISPATCH(PC) \
        inst = &ir[(PC)]; \
        DumpInstruction(); \
        break
    #define LOOP \
        inst = &ir[pc]; \
        DumpInstruction(); \
        for (;;) \
            switch(inst->code)
    #define CASE(Code) \
        case Opcode::Code
#endif

    LOOP {
        CASE(Nop): {
            DISPATCH(++pc);
        }

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
            stack.Append({.b = stack[bp + inst->u.i].b});
            DISPATCH(++pc);
        }
        CASE(LoadInt): {
            stack.Append({.i = stack[bp + inst->u.i].i});
            DISPATCH(++pc);
        }
        CASE(LoadFloat): {
            stack.Append({.d = stack[bp + inst->u.i].d});
            DISPATCH(++pc);
        }
        CASE(LoadString): {
            stack.Append({.str = stack[bp + inst->u.i].str});
            DISPATCH(++pc);
        }
        CASE(LoadType): {
            stack.Append({.type = stack[bp + inst->u.i].type});
            DISPATCH(++pc);
        }
        CASE(StoreBool): {
            stack[bp + inst->u.i].b = stack.ptr[--stack.len].b;
            DISPATCH(++pc);
        }
        CASE(StoreInt): {
            stack[bp + inst->u.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreFloat): {
            stack[bp + inst->u.i].d = stack.ptr[--stack.len].d;
            DISPATCH(++pc);
        }
        CASE(StoreString): {
            stack[bp + inst->u.i].str = stack.ptr[--stack.len].str;
            DISPATCH(++pc);
        }
        CASE(StoreType): {
            stack[bp + inst->u.i].type = stack.ptr[--stack.len].type;
            DISPATCH(++pc);
        }
        CASE(CopyBool): {
            stack[bp + inst->u.i].b = stack.ptr[stack.len - 1].b;
            DISPATCH(++pc);
        }
        CASE(CopyInt): {
            stack[bp + inst->u.i].i = stack.ptr[stack.len - 1].i;
            DISPATCH(++pc);
        }
        CASE(CopyFloat): {
            stack[bp + inst->u.i].d = stack.ptr[stack.len - 1].d;
            DISPATCH(++pc);
        }
        CASE(CopyString): {
            stack[bp + inst->u.i].str = stack.ptr[stack.len - 1].str;
            DISPATCH(++pc);
        }
        CASE(CopyType): {
            stack[bp + inst->u.i].type = stack.ptr[stack.len - 1].type;
            DISPATCH(++pc);
        }

        CASE(LoadGlobalBool): {
            stack.Append({.b = stack[inst->u.i].b});
            DISPATCH(++pc);
        }
        CASE(LoadGlobalInt): {
            stack.Append({.i = stack[inst->u.i].i});
            DISPATCH(++pc);
        }
        CASE(LoadGlobalFloat): {
            stack.Append({.d = stack[inst->u.i].d});
            DISPATCH(++pc);
        }
        CASE(LoadGlobalString): {
            stack.Append({.str = stack[inst->u.i].str});
            DISPATCH(++pc);
        }
        CASE(LoadGlobalType): {
            stack.Append({.type = stack[inst->u.i].type});
            DISPATCH(++pc);
        }
        CASE(StoreGlobalBool): {
            stack[inst->u.i].b = stack.ptr[--stack.len].b;
            DISPATCH(++pc);
        }
        CASE(StoreGlobalInt): {
            stack[inst->u.i].i = stack.ptr[--stack.len].i;
            DISPATCH(++pc);
        }
        CASE(StoreGlobalFloat): {
            stack[inst->u.i].d = stack.ptr[--stack.len].d;
            DISPATCH(++pc);
        }
        CASE(StoreGlobalString): {
            stack[inst->u.i].str = stack.ptr[--stack.len].str;
            DISPATCH(++pc);
        }
        CASE(StoreGlobalType): {
            stack[inst->u.i].type = stack.ptr[--stack.len].type;
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

            stack.Grow(2);
            stack[stack.len++].i = pc;
            stack[stack.len++].i = bp;
            bp = stack.len;

            DISPATCH(pc = func->inst_idx);
        }
        CASE(CallNative): {
            const FunctionInfo *func = inst->u.func;
            RG_ASSERT(func->mode == FunctionInfo::Mode::Native);

            stack.Grow(2);
            stack[stack.len++].i = pc;
            stack[stack.len++].i = bp;
            bp = stack.len;

            pc = func->inst_idx;

            Span<const Value> args;
            if (func->variadic) {
                args.len = func->params.len + (stack[bp - 3].i * 2);
                args.ptr = stack.end() - args.len - 3;

                stack.len -= 2 + args.len;
            } else {
                args.len = func->params.len;
                args.ptr = stack.end() - args.len - 2;

                stack.len -= 1 + args.len;
            }

            Value ret = func->native(this, args);

            pc = stack.ptr[bp - 2].i;
            bp = stack.ptr[bp - 1].i;
            stack[stack.len - 1] = ret;

            if (RG_UNLIKELY(!run))
                return !error;

            DISPATCH(++pc);
        }
        CASE(Return): {
            RG_ASSERT(stack.len == bp + 1);

            Value ret = stack[stack.len - 1];
            stack.len = bp - inst->u.i - 1;
            pc = stack.ptr[bp - 2].i;
            bp = stack.ptr[bp - 1].i;
            stack[stack.len - 1] = ret;

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
            pc++;

            RG_ASSERT(stack.len == program->end_stack_len);
            return true;
        }
    }

    RG_UNREACHABLE();

#undef CASE
#undef LOOP
#undef DISPATCH
}

static void Decode1(const Program &program, Size pc, Size bp, HeapArray<FrameInfo> *out_frames)
{
    FrameInfo frame = {};

    frame.pc = pc;
    frame.bp = bp;
    if (bp) {
        auto func = std::upper_bound(program.functions.begin(), program.functions.end(), pc,
                                     [](Size pc, const FunctionInfo &func) { return pc < func.inst_idx; });
        --func;

        frame.func = &*func;
    }

    const SourceInfo *src = std::upper_bound(program.sources.begin(), program.sources.end(), pc,
                                             [](Size pc, const SourceInfo &src) { return pc < src.lines[0].first_idx; }) - 1;
    if (src >= program.sources.ptr) {
        const SourceInfo::Line *line = std::upper_bound(src->lines.begin(), src->lines.end(), pc,
                                                        [](Size pc, const SourceInfo::Line &line) { return pc < line.first_idx; }) - 1;

        frame.filename = src->filename;
        frame.line = line->line;
    }

    out_frames->Append(frame);
}

void VirtualMachine::DecodeFrames(const VirtualMachine &vm, HeapArray<FrameInfo> *out_frames) const
{
    Size pc = vm.pc;
    Size bp = vm.bp;

    // Walk up call frames
    if (vm.bp) {
        Decode1(*vm.program, pc, bp, out_frames);

        for (;;) {
            pc = vm.stack[bp - 2].i;
            bp = vm.stack[bp - 1].i;

            if (!bp)
                break;

            Decode1(*vm.program, pc, bp, out_frames);
        }
    }

    // Outside funtion
    Decode1(*vm.program, pc, 0, out_frames);
}

void VirtualMachine::DumpInstruction() const
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

        case Opcode::LoadGlobalBool: { LogDebug("[0x%1] LoadGlobalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalInt: { LogDebug("[0x%1] LoadGlobalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalFloat: { LogDebug("[0x%1] LoadGlobalFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalString: { LogDebug("[0x%1] LoadGlobalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalType: { LogDebug("[0x%1] LoadGlobalType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalBool: { LogDebug("[0x%1] StoreGlobalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalInt: { LogDebug("[0x%1] StoreGlobalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalFloat: { LogDebug("[0x%1] StoreGlobalFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalString: { LogDebug("[0x%1] StoreGlobalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalType: { LogDebug("[0x%1] StoreGlobalType @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

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
        case Opcode::Return: { LogDebug("[0x%1] Return (%2)", FmtHex(pc).Pad0(-5), inst.u.i); } break;

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
