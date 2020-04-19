// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "compiler.hh"
#include "vm.hh"

namespace RG {

union Value {
    bool b;
    int64_t i;
    double d;
    const char *str;
};

static void DumpInstruction(Size pc, const Instruction &inst)
{
#if 0
    switch (inst.code) {
        case Opcode::PushBool: { LogDebug("(0x%1) PushBool %2", FmtHex(pc).Pad0(-5), inst.u.b); } break;
        case Opcode::PushInt: { LogDebug("(0x%1) PushInt %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::PushDouble: { LogDebug("(0x%1) PushDouble %2", FmtHex(pc).Pad0(-5), inst.u.d); } break;
        case Opcode::PushString: { LogDebug("(0x%1) PushString %2", FmtHex(pc).Pad0(-5), inst.u.str); } break;
        case Opcode::Pop: { LogDebug("(0x%1) Pop %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::StoreGlobalBool: { LogDebug("(0x%1) StoreGlobalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalInt: { LogDebug("(0x%1) StoreGlobalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalDouble: { LogDebug("(0x%1) StoreGlobalDouble @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreGlobalString: { LogDebug("(0x%1) StoreGlobalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalBool: { LogDebug("(0x%1) LoadGlobalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalInt: { LogDebug("(0x%1) LoadGlobalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalDouble: { LogDebug("(0x%1) LoadGlobalDouble @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadGlobalString: { LogDebug("(0x%1) LoadGlobalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::StoreLocalBool: { LogDebug("(0x%1) StoreLocalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalInt: { LogDebug("(0x%1) StoreLocalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalDouble: { LogDebug("(0x%1) StoreLocalDouble @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::StoreLocalString: { LogDebug("(0x%1) StoreLocalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalBool: { LogDebug("(0x%1) LoadLocalBool @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalInt: { LogDebug("(0x%1) LoadLocalInt @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalDouble: { LogDebug("(0x%1) LoadLocalDouble @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;
        case Opcode::LoadLocalString: { LogDebug("(0x%1) LoadLocalString @%2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        case Opcode::Jump: { LogDebug("(0x%1) Jump 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::BranchIfTrue: { LogDebug("(0x%1) BranchIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::BranchIfFalse: { LogDebug("(0x%1) BranchIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::SkipIfTrue: { LogDebug("(0x%1) SkipIfTrue 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;
        case Opcode::SkipIfFalse: { LogDebug("(0x%1) SkipIfFalse 0x%2", FmtHex(pc).Pad0(-5), FmtHex(pc + inst.u.i).Pad0(-5)); } break;

        case Opcode::Call: { LogDebug("(0x%1) Call 0x%2", FmtHex(pc).Pad0(-5), FmtHex(inst.u.i).Pad0(-5)); } break;
        case Opcode::Return: { LogDebug("(0x%1) Return %2", FmtHex(pc).Pad0(-5), inst.u.i); } break;

        default: { LogDebug("(0x%1) %2", FmtHex(pc).Pad0(-5), OpcodeNames[(int)inst.code]); } break;
    }
#endif
}

int Run(const Program &program)
{
    HeapArray<Value> stack;
    Size pc = 0, bp = 0;
    const Instruction *inst;

#if defined(__GNUC__) || defined(__clang__)
    static const void *dispatch[] = {
        #define OPCODE(Code) && Code,
        #include "opcodes.inc"
    };

    #define DISPATCH(PC) \
        inst = &program.ir[(PC)]; \
        DumpInstruction(pc, *inst); \
        goto *dispatch[(int)inst->code];
    #define LOOP \
        DISPATCH(0)
    #define CASE(Code) \
        Code
#else
    #define DISPATCH(PC) \
        inst = &program.ir[(PC)]; \
        DumpInstruction(pc, *inst); \
        break
    #define LOOP \
        inst = &program.ir[pc]; \
        DumpInstruction(pc, *inst); \
        for (;;) \
            switch(inst->code)
    #define CASE(Code) \
        case Opcode::Code
#endif

    LOOP {
        CASE(PushNull): {
            stack.Append(Value());
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
        CASE(PushDouble): {
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
        CASE(Duplicate): {
            stack.Append(stack[stack.len - 1]);
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
        CASE(LoadGlobalDouble): {
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
        CASE(StoreGlobalDouble): {
            stack[inst->u.i].d = stack.ptr[--stack.len].d;
            DISPATCH(++pc);
        }
        CASE(StoreGlobalString): {
            stack[inst->u.i].str = stack.ptr[--stack.len].str;
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
        CASE(LoadLocalDouble): {
            stack.Append({.d = stack[bp + inst->u.i].d});
            DISPATCH(++pc);
        }
        CASE(LoadLocalString): {
            stack.Append({.str = stack[bp + inst->u.i].str});
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
        CASE(StoreLocalDouble): {
            stack[bp + inst->u.i].d = stack.ptr[--stack.len].d;
            DISPATCH(++pc);
        }
        CASE(StoreLocalString): {
            stack[bp + inst->u.i].str = stack.ptr[--stack.len].str;
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
            stack[--stack.len - 1].i = i1 / i2;
            DISPATCH(++pc);
        }
        CASE(ModuloInt): {
            int64_t i1 = stack[stack.len - 2].i;
            int64_t i2 = stack[stack.len - 1].i;
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

        CASE(NegateDouble): {
            double d = stack[stack.len - 1].d;
            stack[stack.len - 1].d = -d;
            DISPATCH(++pc);
        }
        CASE(AddDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 + d2;
            DISPATCH(++pc);
        }
        CASE(SubstractDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 - d2;
            DISPATCH(++pc);
        }
        CASE(MultiplyDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 * d2;
            DISPATCH(++pc);
        }
        CASE(DivideDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].d = d1 / d2;
            DISPATCH(++pc);
        }
        CASE(EqualDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 == d2);
            DISPATCH(++pc);
        }
        CASE(NotEqualDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 != d2);
            DISPATCH(++pc);
        }
        CASE(GreaterThanDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 > d2);
            DISPATCH(++pc);
        }
        CASE(GreaterOrEqualDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 >= d2);
            DISPATCH(++pc);
        }
        CASE(LessThanDouble): {
            double d1 = stack[stack.len - 2].d;
            double d2 = stack[stack.len - 1].d;
            stack[--stack.len - 1].b = (d1 < d2);
            DISPATCH(++pc);
        }
        CASE(LessOrEqualDouble): {
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
            stack.ptr[stack.len++].i = pc + 1;
            stack.ptr[stack.len++].i = bp;
            bp = stack.len;
            DISPATCH(pc = (Size)inst->u.i);
        }
        CASE(Return): {
            Value ret = stack.ptr[stack.len - 1];
            pc = stack.ptr[stack.len - 3].i;
            bp = stack.ptr[stack.len - 2].i;
            stack.len -= 2 + inst->u.i;
            stack[stack.len - 1] = ret;
            DISPATCH(pc);
        }

        // This will be removed once we get functions, but in the mean time
        // I need to output things somehow!
        CASE(Print): {
            switch (inst->u.type) {
                case Type::Null: { Print("null"); } break;
                case Type::Bool: { Print("%1", stack.ptr[stack.len - 1].b); } break;
                case Type::Int: { Print("%1", stack.ptr[stack.len - 1].i); } break;
                case Type::Double: { Print("%1", stack.ptr[stack.len - 1].d); } break;
                case Type::String: { Print("%1", stack.ptr[stack.len - 1].str); } break;
            }

            DISPATCH(++pc);
        }

        CASE(Exit): {
            int code = (int)stack.ptr[--stack.len].i;

            RG_ASSERT(stack.len == program.globals.len);
            return (int)code;
        }
    }

#undef CASE
#undef LOOP
#undef DISPATCH
}

}
