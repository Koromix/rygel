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
}

static Value DoPrint(VirtualMachine *vm, Span<const Value> args)
{
    RG_ASSERT(args.len % 2 == 0);

    for (Size i = 0; i < args.len; i += 2) {
        switch (args[i + 1].type->primitive) {
            case PrimitiveType::Null: { Print("null"); } break;
            case PrimitiveType::Bool: { Print("%1", args[i].b); } break;
            case PrimitiveType::Int: { Print("%1", args[i].i); } break;
            case PrimitiveType::Float: { Print("%1", args[i].d); } break;
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

}
