// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "compiler.hh"
#include "debug.hh"
#include "vm.hh"

namespace RG {

class Interpreter {
    const Program *program;
    Span<const Instruction> ir;
    const DebugInfo *debug; // Can be NULL

    HeapArray<Value> stack;
    Size pc = 0;
    Size bp = 0;
    const Instruction *inst;

public:
    int Run(const Program &program, const DebugInfo *debug);

private:
    void DumpInstruction();

    template <typename... Args>
    void FatalError(const char *fmt, Args... args)
    {
        HeapArray<FrameInfo> frames;
        DecodeFrames(*program, debug, stack, pc, bp, &frames);

        ReportRuntimeError(frames, fmt, args...);
    }
};

int Interpreter::Run(const Program &program, const DebugInfo *debug)
{
    this->program = &program;
    ir = program.ir;
    this->debug = debug;

    stack.Clear();
    pc = 0;
    bp = 0;

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
        DISPATCH(0)
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
                return 1;
            }
            stack[--stack.len - 1].i = i1 / i2;
            DISPATCH(++pc);
        }
        CASE(ModuloInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            if (RG_UNLIKELY(i2 == 0)) {
                FatalError("Division by 0 is illegal");
                return 1;
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
        CASE(NotInt): {
            int64_t i = stack[stack.len - 1].i;
            stack[stack.len - 1].i = ~i;
            DISPATCH(++pc);
        }
        CASE(LeftShiftInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].i = i1 << i2;
            DISPATCH(++pc);
        }
        CASE(RightShiftInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
            stack[--stack.len - 1].i = (int64_t)((uint64_t)i1 >> i2);
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
            pc = stack.ptr[bp - 2].i + 1;
            bp = stack.ptr[bp - 1].i;
            stack[stack.len - 1] = ret;
            DISPATCH(pc);
        }
        CASE(ReturnNull): {
            RG_ASSERT(stack.len == bp);

            stack.len = bp - inst->u.i - 2;
            pc = stack.ptr[bp - 2].i + 1;
            bp = stack.ptr[bp - 1].i;
            DISPATCH(pc);
        }

        // This will be removed once we get functions, but in the mean time
        // I need to output things somehow!
        CASE(Print): {
            int64_t remain = inst->u.i;

            Size count = (Size)(remain & 0x1F);
            Size pop = (Size)((remain >> 5) & 0x1F);
            remain >>= 10;

            Size stack_offset = stack.len - pop;
            for (Size i = 0; i < count; i++) {
                Type type = (Type)(remain & 0x7);
                remain >>= 3;

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

#ifndef NDEBUG
            if (inst->u.b) {
                Size good_stack_len = 0;
                for (const VariableInfo &var: program.globals) {
                    good_stack_len += (var.type != Type::Null);
                }
                RG_ASSERT(stack.len == good_stack_len);
            }
#endif

            return (int)code;
        }
    }

#undef CASE
#undef LOOP
#undef DISPATCH
}

void Interpreter::DumpInstruction()
{
#if 0
    switch (inst->code) {
        case Opcode::PushBool: { LogDebug("(0x%1) PushBool %2", FmtHex(pc).Pad0(-5), inst->u.b); } break;
        case Opcode::PushInt: { LogDebug("(0x%1) PushInt %2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::PushFloat: { LogDebug("(0x%1) PushFloat %2", FmtHex(pc).Pad0(-5), inst->u.d); } break;
        case Opcode::PushString: { LogDebug("(0x%1) PushString %2", FmtHex(pc).Pad0(-5), inst->u.str); } break;
        case Opcode::Pop: { LogDebug("(0x%1) Pop %2", FmtHex(pc).Pad0(-5), inst->u.i); } break;

        case Opcode::LoadBool: { LogDebug("(0x%1) LoadBool @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::LoadInt: { LogDebug("(0x%1) LoadInt @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::LoadFloat: { LogDebug("(0x%1) LoadFloat @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::LoadString: { LogDebug("(0x%1) LoadString @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreBool: { LogDebug("(0x%1) StoreBool @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreInt: { LogDebug("(0x%1) StoreInt @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreFloat: { LogDebug("(0x%1) StoreFloat @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreString: { LogDebug("(0x%1) StoreString @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::CopyBool: { LogDebug("(0x%1) CopyBool @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::CopyInt: { LogDebug("(0x%1) CopyInt @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::CopyFloat: { LogDebug("(0x%1) CopyFloat @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::CopyString: { LogDebug("(0x%1) CopyString @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;

        case Opcode::LoadGlobalBool: { LogDebug("(0x%1) LoadGlobalBool @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::LoadGlobalInt: { LogDebug("(0x%1) LoadGlobalInt @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::LoadGlobalFloat: { LogDebug("(0x%1) LoadGlobalFloat @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::LoadGlobalString: { LogDebug("(0x%1) LoadGlobalString @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreGlobalBool: { LogDebug("(0x%1) StoreGlobalBool @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreGlobalInt: { LogDebug("(0x%1) StoreGlobalInt @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreGlobalFloat: { LogDebug("(0x%1) StoreGlobalFloat @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::StoreGlobalString: { LogDebug("(0x%1) StoreGlobalString @%2", FmtHex(pc).Pad0(-5), inst->u.i); } break;

        case Opcode::Jump: { LogDebug("(0x%1) Jump 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst->u.i).Pad0(-5)); } break;
        case Opcode::BranchIfTrue: { LogDebug("(0x%1) BranchIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst->u.i).Pad0(-5)); } break;
        case Opcode::BranchIfFalse: { LogDebug("(0x%1) BranchIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst->u.i).Pad0(-5)); } break;
        case Opcode::SkipIfTrue: { LogDebug("(0x%1) SkipIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst->u.i).Pad0(-5)); } break;
        case Opcode::SkipIfFalse: { LogDebug("(0x%1) SkipIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst->u.i).Pad0(-5)); } break;

        case Opcode::Call: { LogDebug("(0x%1) Call 0x%2", FmtHex(pc).Pad0(-5), FmtHex(inst->u.i).Pad0(-5)); } break;
        case Opcode::Return: { LogDebug("(0x%1) Return %2", FmtHex(pc).Pad0(-5), inst->u.i); } break;
        case Opcode::ReturnNull: { LogDebug("(0x%1) ReturnNull %2", FmtHex(pc).Pad0(-5), inst->u.i); } break;

        case Opcode::Print: { LogDebug("(0x%1) Print %2", FmtHex(pc).Pad0(-5), inst->u.i & 0x1F); } break;

        default: { LogDebug("(0x%1) %2", FmtHex(pc).Pad0(-5), OpcodeNames[(int)inst->code]); } break;
    }
#endif
}

int Run(const Program &program, const DebugInfo *debug)
{
    Interpreter interp;
    return interp.Run(program, debug);
}

}
