// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "compiler.hh"
#include "std.hh"

namespace RG {

void ImportAll(Compiler *out_compiler)
{
    ImportPrint(out_compiler);
    ImportMath(out_compiler);
}

static Value DoPrint(VirtualMachine *vm, Span<const Value> args)
{
    RG_ASSERT(args.len % 2 == 0);

    for (Size i = 0; i < args.len; i += 2) {
        switch (args[i + 1].type->primitive) {
            case PrimitiveType::Null: { Print("null"); } break;
            case PrimitiveType::Bool: { Print("%1", args[i].b); } break;
            case PrimitiveType::Int: { Print("%1", args[i].i); } break;
            case PrimitiveType::Float: { Print("%1", FmtDouble(args[i].d, 1, INT_MAX)); } break;
            case PrimitiveType::String: { Print("%1", args[i].str); } break;
            case PrimitiveType::Type: { Print("%1", args[i].type->signature); } break;
        }
    }

    return Value();
}

void ImportPrint(Compiler *out_compiler)
{
    out_compiler->AddFunction("print(...)", DoPrint);
    out_compiler->AddFunction("printLn(...)", [](VirtualMachine *vm, Span<const Value> args) {
        DoPrint(vm, args);
        PrintLn();

        return Value();
    });
}

void ImportMath(Compiler *out_compiler)
{
    out_compiler->AddGlobal("PI", PrimitiveType::Float, Value {.d = 3.141592653589793});
    out_compiler->AddGlobal("E", PrimitiveType::Float, Value {.d = 2.718281828459045});
    out_compiler->AddGlobal("TAU", PrimitiveType::Float, Value {.d = 6.283185307179586});

    out_compiler->AddFunction("isNormal(Float): Bool", [](VirtualMachine *, Span<const Value> args) { return Value {.b = std::isnormal(args[0].d)}; });
    out_compiler->AddFunction("isInfinity(Float): Bool", [](VirtualMachine *, Span<const Value> args) { return Value {.b = std::isinf(args[0].d)}; });
    out_compiler->AddFunction("isNaN(Float): Bool", [](VirtualMachine *, Span<const Value> args) { return Value {.b = std::isnan(args[0].d)}; });

    out_compiler->AddFunction("ceil(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = ceil(args[0].d)}; });
    out_compiler->AddFunction("floor(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = floor(args[0].d)}; });
    out_compiler->AddFunction("round(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = round(args[0].d)}; });
    out_compiler->AddFunction("abs(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = fabs(args[0].d)}; });

    out_compiler->AddFunction("exp(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = exp(args[0].d)}; });
    out_compiler->AddFunction("ln(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = log(args[0].d)}; });
    out_compiler->AddFunction("log2(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = log2(args[0].d)}; });
    out_compiler->AddFunction("log10(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = log10(args[0].d)}; });
    out_compiler->AddFunction("pow(Float, Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = pow(args[0].d, args[1].d)}; });
    out_compiler->AddFunction("sqrt(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = sqrt(args[0].d)}; });
    out_compiler->AddFunction("cbrt(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = cbrt(args[0].d)}; });

    out_compiler->AddFunction("cos(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = cos(args[0].d)}; });
    out_compiler->AddFunction("sin(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = sin(args[0].d)}; });
    out_compiler->AddFunction("tan(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = tan(args[0].d)}; });
    out_compiler->AddFunction("acos(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = acos(args[0].d)}; });
    out_compiler->AddFunction("asin(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = asin(args[0].d)}; });
    out_compiler->AddFunction("atan(Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = atan(args[0].d)}; });
    out_compiler->AddFunction("atan2(Float, Float): Float", [](VirtualMachine *, Span<const Value> args) { return Value {.d = atan2(args[0].d, args[1].d)}; });
}

}
