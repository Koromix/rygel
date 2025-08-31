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

#pragma once

#include "src/core/base/base.hh"
#include "program.hh"

namespace K {

class bk_Parser;
struct bk_TokenizedFile;

struct bk_CompileReport {
    bool unexpected_eof;
    int depth;
};

class bk_Compiler {
    K_DELETE_COPY(bk_Compiler)

    bk_Parser *parser;

public:
    bk_Compiler(bk_Program *out_program);
    ~bk_Compiler();

    bool Compile(const bk_TokenizedFile &file, bk_CompileReport *out_report = nullptr);
    bool Compile(Span<const char> code, const char *filename, bk_CompileReport *out_report = nullptr);

    void AddFunction(const char *prototype, unsigned int flags, std::function<bk_NativeFunction> native);
    void AddGlobal(const char *name, const bk_TypeInfo *type, Span<const bk_PrimitiveValue> values);
    void AddOpaque(const char *name);
};

#define BK_ADD_FUNCTION(Compiler, Signature, Pure, Code) \
    (Compiler).AddFunction((Signature), (Pure), [&]([[maybe_unused]] bk_VirtualMachine *vm, \
                                                    [[maybe_unused]] Span<const bk_PrimitiveValue> args, \
                                                    [[maybe_unused]] Span<bk_PrimitiveValue> ret) Code)

}
