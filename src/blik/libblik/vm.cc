// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "program.hh"
#include "vm.hh"

namespace RG {

bool VirtualMachine::Run(int *out_exit_code)
{
    const Instruction *inst;

    ir = program->ir;
    fatal = false;

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
                FatalError("Division by 0 is illegal");
                return false;
            }
            stack[--stack.len - 1].i = i1 / i2;
            DISPATCH(++pc);
        }
        CASE(ModuloInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            if (RG_UNLIKELY(i2 == 0)) {
                FatalError("Division by 0 is illegal");
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
            stack.Grow(2);
            stack.ptr[stack.len++].i = pc;
            stack.ptr[stack.len++].i = bp;
            bp = stack.len;
            DISPATCH(pc = (Size)inst->u.i);
        }
        CASE(Return): {
            RG_ASSERT(stack.len == bp + 1);

            Value ret = stack.ptr[stack.len - 1];
            stack.len = bp - inst->u.i - 1;
            pc = stack.ptr[bp - 2].i;
            bp = stack.ptr[bp - 1].i;
            stack[stack.len - 1] = ret;
            DISPATCH(++pc);
        }
        CASE(ReturnNull): {
            RG_ASSERT(stack.len == bp);

            stack.len = bp - inst->u.i - 2;
            pc = stack.ptr[bp - 2].i;
            bp = stack.ptr[bp - 1].i;
            DISPATCH(++pc);
        }
        CASE(Invoke): {
            RG_ASSERT(stack.len == bp);

            NativeFunction *native = (NativeFunction *)(inst->u.payload & 0x1FFFFFFFFFFFFFFull);
            Size ret_pop = (Size)(inst->u.payload >> 57) & 0x3F;

            Span<const Value> args = stack.Take(stack.len - ret_pop - 2, ret_pop);

            Value ret = (*native)(this, args);
            stack.len -= ret_pop + 1;

            pc = stack.ptr[bp - 2].i;
            bp = stack.ptr[bp - 1].i;

            if (RG_UNLIKELY(fatal)) {
                stack.len--;
                return false;
            }
            stack[stack.len - 1] = ret;

            DISPATCH(++pc);
        }
        CASE(InvokeNull): {
            RG_ASSERT(stack.len == bp);

            NativeFunction *native = (NativeFunction *)(inst->u.payload & 0x1FFFFFFFFFFFFFFull);
            Size ret_pop = (Size)(inst->u.payload >> 57) & 0x3F;

            Span<const Value> args = stack.Take(stack.len - ret_pop - 2, ret_pop);

            (*native)(this, args);
            stack.len -= ret_pop + 2;

            pc = stack.ptr[bp - 2].i;
            bp = stack.ptr[bp - 1].i;

            if (RG_UNLIKELY(fatal))
                return false;

            DISPATCH(++pc);
        }

        // This will be removed once we get functions, but in the mean time
        // I need to output things somehow!
        CASE(Print): {
            uint64_t payload = inst->u.payload;

            Size count = (Size)(payload & 0x1F);
            Size pop = (Size)((payload >> 5) & 0x1F);
            payload >>= 10;

            Size stack_offset = stack.len - pop;
            for (Size i = 0; i < count; i++) {
                Type type = (Type)(payload & 0x7);
                payload >>= 3;

                switch (type) {
                    case Type::Null: { Print("null"); } break;
                    case Type::Bool: { Print("%1", stack[stack_offset++].b); } break;
                    case Type::Int: { Print("%1", stack[stack_offset++].i); } break;
                    case Type::Float: { Print("%1", stack[stack_offset++].d); } break;
                    case Type::String: { Print("%1", stack[stack_offset++].str); } break;
                }
            }

            stack.len -= pop;

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

        CASE(Exit): {
            int code = (int)stack.ptr[--stack.len].i;

            RG_ASSERT(stack.len == program->end_stack_len || !inst->u.b);

            *out_exit_code = code;
            return true;
        }
    }

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
        const SourceInfo::LineInfo *line = std::upper_bound(src->lines.begin(), src->lines.end(), pc,
                                                            [](Size pc, const SourceInfo::LineInfo &line) { return pc < line.first_idx; }) - 1;

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
        case Opcode::Pop: { LogDebug("[0x%1] Pop %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::LoadBool: { LogDebug("[0x%1] LoadBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadInt: { LogDebug("[0x%1] LoadInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadFloat: { LogDebug("[0x%1] LoadFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadString: { LogDebug("[0x%1] LoadString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreBool: { LogDebug("[0x%1] StoreBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreInt: { LogDebug("[0x%1] StoreInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreFloat: { LogDebug("[0x%1] StoreFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreString: { LogDebug("[0x%1] StoreString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyBool: { LogDebug("[0x%1] CopyBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyInt: { LogDebug("[0x%1] CopyInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyFloat: { LogDebug("[0x%1] CopyFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::CopyString: { LogDebug("[0x%1] CopyString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::LoadGlobalBool: { LogDebug("[0x%1] LoadGlobalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalInt: { LogDebug("[0x%1] LoadGlobalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalFloat: { LogDebug("[0x%1] LoadGlobalFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalString: { LogDebug("[0x%1] LoadGlobalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalBool: { LogDebug("[0x%1] StoreGlobalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalInt: { LogDebug("[0x%1] StoreGlobalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalFloat: { LogDebug("[0x%1] StoreGlobalFloat @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalString: { LogDebug("[0x%1] StoreGlobalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::Jump: { LogDebug("[0x%1] Jump 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::BranchIfTrue: { LogDebug("[0x%1] BranchIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::BranchIfFalse: { LogDebug("[0x%1] BranchIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::SkipIfTrue: { LogDebug("[0x%1] SkipIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::SkipIfFalse: { LogDebug("[0x%1] SkipIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;

        case Opcode::Call: { LogDebug("[0x%1] Call 0x%2", FmtHex(pc).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;
        case Opcode::Return: { LogDebug("[0x%1] Return %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::ReturnNull: { LogDebug("[0x%1] ReturnNull %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::Invoke: { LogDebug("[0x%1] Invoke 0x%2", FmtHex(pc).Pad0(-5), FmtHex(inst.u.payload & 0x1FFFFFFFFFFFFFFull).Pad0(-5)); } break;
        case Opcode::InvokeNull: { LogDebug("[0x%1] InvokeNull 0x%2", FmtHex(pc).Pad0(-5), FmtHex(inst.u.payload & 0x1FFFFFFFFFFFFFFull).Pad0(-5)); } break;

        case Opcode::Print: { LogDebug("[0x%1] Print %2", FmtHex(pc).Pad0(-5), inst.u.payload & 0x1F); } break;

        default: { LogDebug("[0x%1] %2", FmtHex(pc).Pad0(-5), OpcodeNames[(int)inst.code]); } break;
    }
#endif
}

bool Run(const Program &program, int *out_exit_code)
{
    VirtualMachine vm(&program);
    return vm.Run(out_exit_code);
}

}
