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
#include "compiler.hh"
#include "program.hh"
#include "std.hh"
#include "vm.hh"

namespace RG {

void bk_ImportAll(bk_Compiler *out_compiler)
{
    bk_ImportPrint(out_compiler);
    bk_ImportMath(out_compiler);
    bk_ImportRandom(out_compiler);
}

void bk_ImportPrint(bk_Compiler *out_compiler)
{
    BK_ADD_FUNCTION(*out_compiler, "print(...)", 0, { bk_DoPrint(vm, args, false); });
    BK_ADD_FUNCTION(*out_compiler, "printLn(...)", 0, { bk_DoPrint(vm, args, false); PrintLn(); });
    BK_ADD_FUNCTION(*out_compiler, "log(...)", 0, { bk_DoPrint(vm, args, true); PrintLn(); });

    BK_ADD_FUNCTION(*out_compiler, "debug(): Bool", 0, {
        unsigned int flags = vm->GetFlags();
        ret[0].b = flags & (int)bk_RunFlag::Debug;
    });
    BK_ADD_FUNCTION(*out_compiler, "debug(Bool)", 0, {
        unsigned int flags = vm->GetFlags();
        flags = ApplyMask(flags, bk_RunFlag::Debug, args[0].b);
        vm->SetFlags(flags);
    });
}

void bk_ImportMath(bk_Compiler *out_compiler)
{
    out_compiler->AddGlobal("PI", bk_FloatType, { { .d = 3.141592653589793 } });
    out_compiler->AddGlobal("E", bk_FloatType, { { .d = 2.718281828459045 } });
    out_compiler->AddGlobal("TAU", bk_FloatType, { { .d = 6.283185307179586 } });

    BK_ADD_FUNCTION(*out_compiler, "isNormal(Float): Bool", (int)bk_FunctionFlag::Pure, { ret[0].b = std::isnormal(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "isInfinity(Float): Bool", (int)bk_FunctionFlag::Pure, { ret[0].b = std::isinf(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "isNaN(Float): Bool", (int)bk_FunctionFlag::Pure, { ret[0].b = std::isnan(args[0].d); });

    BK_ADD_FUNCTION(*out_compiler, "ceil(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = ceil(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "floor(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = floor(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "round(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = round(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "abs(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = fabs(args[0].d); });

    BK_ADD_FUNCTION(*out_compiler, "exp(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = exp(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "ln(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = log(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "log2(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = log2(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "log10(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = log10(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "pow(Float, Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = pow(args[0].d, args[1].d); });
    BK_ADD_FUNCTION(*out_compiler, "sqrt(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = sqrt(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "cbrt(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = cbrt(args[0].d); });

    BK_ADD_FUNCTION(*out_compiler, "cos(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = cos(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "sin(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = sin(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "tan(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = tan(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "acos(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = acos(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "asin(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = asin(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "atan(Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = atan(args[0].d); });
    BK_ADD_FUNCTION(*out_compiler, "atan2(Float, Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = atan2(args[0].d, args[1].d); });

    BK_ADD_FUNCTION(*out_compiler, "min(Int, Int): Int", (int)bk_FunctionFlag::Pure, { ret[0].i = std::min(args[0].i, args[1].i); });
    BK_ADD_FUNCTION(*out_compiler, "min(Float, Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = std::min(args[0].d, args[1].d); });
    BK_ADD_FUNCTION(*out_compiler, "max(Int, Int): Int", (int)bk_FunctionFlag::Pure, { ret[0].i = std::max(args[0].i, args[1].i); });
    BK_ADD_FUNCTION(*out_compiler, "max(Float, Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = std::max(args[0].d, args[1].d); });
    BK_ADD_FUNCTION(*out_compiler, "clamp(Int, Int, Int): Int", (int)bk_FunctionFlag::Pure, { ret[0].i = std::clamp(args[0].i, args[1].i, args[2].i); });
    BK_ADD_FUNCTION(*out_compiler, "clamp(Float, Float, Float): Float", (int)bk_FunctionFlag::Pure, { ret[0].d = std::clamp(args[0].d, args[1].d, args[2].d); });
}

void bk_ImportRandom(bk_Compiler *out_compiler)
{
    BK_ADD_FUNCTION(*out_compiler, "randInt(Int, Int): Int", 0, { ret[0].i = GetRandomInt64(args[0].i, args[1].i); });
}

static Size PrintValue(bk_VirtualMachine *vm, const bk_TypeInfo *type, Size offset, bool quote)
{
    switch (type->primitive) {
        case bk_PrimitiveKind::Null: { fputs("null", stdout); } break;
        case bk_PrimitiveKind::Boolean: { Print("%1", vm->stack[offset++].b); } break;
        case bk_PrimitiveKind::Integer: { Print("%1", vm->stack[offset++].i); } break;
        case bk_PrimitiveKind::Float: { Print("%1", FmtDouble(vm->stack[offset++].d, 1, INT_MAX)); } break;
        case bk_PrimitiveKind::String: {
            const char *str = vm->stack[offset++].str;
            str = str ? str : "";

            if (quote) {

                fputc('"', stdout);
                for (;;) {
                    Size next = strcspn(str, "\"\r\n\t\f\v\a\b\x1B");

                    StdOut->Write(str, next);

                    if (!str[next])
                        break;
                    switch (str[next]) {
                        case '\"': { fputs("\\\"", stdout); } break;
                        case '\r': { fputs("\\r", stdout); } break;
                        case '\n': { fputs("\\n", stdout); } break;
                        case '\t': { fputs("\\t", stdout); } break;
                        case '\f': { fputs("\\f", stdout); } break;
                        case '\v': { fputs("\\v", stdout); } break;
                        case '\a': { fputs("\\a", stdout); } break;
                        case '\b': { fputs("\\b", stdout); } break;
                        case '\x1B': { fputs("\\e", stdout); } break;
                    }

                    str += next + 1;
                }
                fputc('"', stdout);
            } else {
                fputs(str, stdout);
            }
        } break;
        case bk_PrimitiveKind::Type: { fputs(vm->stack[offset++].type->signature, stdout); } break;
        case bk_PrimitiveKind::Function: { fputs(vm->stack[offset++].func->prototype, stdout); } break;
        case bk_PrimitiveKind::Array: {
            const bk_ArrayTypeInfo *array_type = type->AsArrayType();

            fputc('[', stdout);
            if (array_type->len) {
                offset = PrintValue(vm, array_type->unit_type, offset, true);
                for (Size i = 1; i < array_type->len; i++) {
                    fputs(", ", stdout);
                    offset = PrintValue(vm, array_type->unit_type, offset, true);
                }
            }
            fputc(']', stdout);
        } break;
        case bk_PrimitiveKind::Record: {
            const bk_RecordTypeInfo *record_type = type->AsRecordType();

            Print("%1(", record_type->signature);
            if (record_type->members.len) {
                Print("%1 = ", record_type->members[0].name);
                offset = PrintValue(vm, record_type->members[0].type, offset, true);
                for (Size i = 1; i < record_type->members.len; i++) {
                    fputs(", ", stdout);
                    Print("%1 = ", record_type->members[i].name);
                    offset = PrintValue(vm, record_type->members[i].type, offset, true);
                }
            }
            fputc(')', stdout);
        } break;
        case bk_PrimitiveKind::Enum: {
            const bk_EnumTypeInfo *enum_type = type->AsEnumType();
            int64_t value = vm->stack[offset++].i;

            if (value >= 0 && value < enum_type->labels.len) [[likely]] {
                const bk_EnumTypeInfo::Label &label = enum_type->labels[value];
                fputs(label.name, stdout);
            } else {
                // This should never happen, except for cosmic bit flips
                Print("<invalid> (%1)", value);
            }
        } break;
        case bk_PrimitiveKind::Opaque: { Print("0x%1", FmtArg(vm->stack[offset++].opaque).Pad0(-RG_SIZE(void *) * 2)); } break;
    }

    return offset;
}

void bk_DoPrint(bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args, bool quote)
{
    for (Size i = 0; i < args.len;) {
        const bk_TypeInfo *type = args[i++].type;

        if (type->PassByReference()) {
            PrintValue(vm, type, args[i].i, quote);
        } else {
            PrintValue(vm, type, args.ptr - vm->stack.ptr + i, quote);
        }

        i += type->size;
    }
}

}
