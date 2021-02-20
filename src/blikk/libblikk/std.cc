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
#include "compiler.hh"
#include "program.hh"
#include "std.hh"
#include "vm.hh"

namespace RG {

void bk_ImportAll(bk_Compiler *out_compiler)
{
    bk_ImportPrint(out_compiler);
    bk_ImportMath(out_compiler);
}

static Size PrintValue(bk_VirtualMachine *vm, const bk_TypeInfo *type, Size offset, bool quote)
{
    switch (type->primitive) {
        case bk_PrimitiveKind::Null: { fputs("null", stdout); offset++; } break;
        case bk_PrimitiveKind::Boolean: { Print("%1", vm->stack[offset++].b); } break;
        case bk_PrimitiveKind::Integer: { Print("%1", vm->stack[offset++].i); } break;
        case bk_PrimitiveKind::Float: { Print("%1", FmtDouble(vm->stack[offset++].d, 1, INT_MAX)); } break;
        case bk_PrimitiveKind::String: {
            if (quote) {
                const char *str = vm->stack[offset++].str;

                fputc('"', stdout);
                for (;;) {
                    Size next = strcspn(str, "\"\r\n\t\f\v\a\b\x1B");

                    fwrite(str, 1, next, stdout);

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
                fputs(vm->stack[offset++].str, stdout);
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
        }
    }

    return offset;
}

static void DoPrint(bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args, bool quote)
{
    RG_ASSERT(args.len % 2 == 0);

    for (Size i = 0; i < args.len; i++) {
        const bk_TypeInfo *type = args[i++].type;

        if (type->PassByReference()) {
            PrintValue(vm, type, args[i].i, quote);
        } else {
            PrintValue(vm, type, args.ptr - vm->stack.ptr + i, quote);
        }
    }
}

void bk_ImportPrint(bk_Compiler *out_compiler)
{
    out_compiler->AddFunction("print(...)", [](bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args) {
        DoPrint(vm, args, false);
        return bk_PrimitiveValue();
    });
    out_compiler->AddFunction("printLn(...)", [](bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args) {
        DoPrint(vm, args, false);
        PrintLn();

        return bk_PrimitiveValue();
    });

    out_compiler->AddFunction("debug(...)", [](bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args) {
        DoPrint(vm, args, true);
        PrintLn();
        return bk_PrimitiveValue();
    });
}

void bk_ImportMath(bk_Compiler *out_compiler)
{
    out_compiler->AddGlobal("PI", bk_FloatType, {{.d = 3.141592653589793}});
    out_compiler->AddGlobal("E", bk_FloatType, {{.d = 2.718281828459045}});
    out_compiler->AddGlobal("TAU", bk_FloatType, {{.d = 6.283185307179586}});

    out_compiler->AddFunction("isNormal(Float): Bool", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.b = std::isnormal(args[0].d)}; });
    out_compiler->AddFunction("isInfinity(Float): Bool", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.b = std::isinf(args[0].d)}; });
    out_compiler->AddFunction("isNaN(Float): Bool", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.b = std::isnan(args[0].d)}; });

    out_compiler->AddFunction("ceil(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = ceil(args[0].d)}; });
    out_compiler->AddFunction("floor(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = floor(args[0].d)}; });
    out_compiler->AddFunction("round(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = round(args[0].d)}; });
    out_compiler->AddFunction("abs(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = fabs(args[0].d)}; });

    out_compiler->AddFunction("exp(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = exp(args[0].d)}; });
    out_compiler->AddFunction("ln(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = log(args[0].d)}; });
    out_compiler->AddFunction("log2(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = log2(args[0].d)}; });
    out_compiler->AddFunction("log10(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = log10(args[0].d)}; });
    out_compiler->AddFunction("pow(Float, Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = pow(args[0].d, args[1].d)}; });
    out_compiler->AddFunction("sqrt(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = sqrt(args[0].d)}; });
    out_compiler->AddFunction("cbrt(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = cbrt(args[0].d)}; });

    out_compiler->AddFunction("cos(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = cos(args[0].d)}; });
    out_compiler->AddFunction("sin(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = sin(args[0].d)}; });
    out_compiler->AddFunction("tan(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = tan(args[0].d)}; });
    out_compiler->AddFunction("acos(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = acos(args[0].d)}; });
    out_compiler->AddFunction("asin(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = asin(args[0].d)}; });
    out_compiler->AddFunction("atan(Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = atan(args[0].d)}; });
    out_compiler->AddFunction("atan2(Float, Float): Float", [](bk_VirtualMachine *, Span<const bk_PrimitiveValue> args) { return bk_PrimitiveValue {.d = atan2(args[0].d, args[1].d)}; });
}

}
