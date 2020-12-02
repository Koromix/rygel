// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "compiler.hh"
#include "std.hh"

namespace RG {

void bk_ImportAll(bk_Compiler *out_compiler)
{
    bk_ImportPrint(out_compiler);
    bk_ImportMath(out_compiler);
}

static bk_PrimitiveValue DoPrint(bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args)
{
    RG_ASSERT(args.len % 2 == 0);

    for (Size i = 0; i < args.len; i += 2) {
        switch (args[i].type->primitive) {
            case bk_PrimitiveType::Null: { Print("null"); } break;
            case bk_PrimitiveType::Bool: { Print("%1", args[i + 1].b); } break;
            case bk_PrimitiveType::Int: { Print("%1", args[i + 1].i); } break;
            case bk_PrimitiveType::Float: { Print("%1", FmtDouble(args[i + 1].d, 1, INT_MAX)); } break;
            case bk_PrimitiveType::Type: { Print("%1", args[i + 1].type->signature); } break;
        }
    }

    return bk_PrimitiveValue();
}

void bk_ImportPrint(bk_Compiler *out_compiler)
{
    out_compiler->AddFunction("print(...)", DoPrint);
    out_compiler->AddFunction("printLn(...)", [](bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args) {
        DoPrint(vm, args);
        PrintLn();

        return bk_PrimitiveValue();
    });
}

void bk_ImportMath(bk_Compiler *out_compiler)
{
    out_compiler->AddGlobal("PI", bk_PrimitiveType::Float, bk_PrimitiveValue {.d = 3.141592653589793});
    out_compiler->AddGlobal("E", bk_PrimitiveType::Float, bk_PrimitiveValue {.d = 2.718281828459045});
    out_compiler->AddGlobal("TAU", bk_PrimitiveType::Float, bk_PrimitiveValue {.d = 6.283185307179586});

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
