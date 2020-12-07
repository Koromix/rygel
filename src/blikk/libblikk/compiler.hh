// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

class bk_Parser;
struct bk_TokenizedFile;

struct bk_CompileReport {
    bool unexpected_eof;
    int depth;
};

class bk_Compiler {
    RG_DELETE_COPY(bk_Compiler)

    bk_Parser *parser;

public:
    bk_Compiler(bk_Program *out_program);
    ~bk_Compiler();

    bool Compile(const bk_TokenizedFile &file, bk_CompileReport *out_report = nullptr);
    bool Compile(Span<const char> code, const char *filename, bk_CompileReport *out_report = nullptr);

    void AddFunction(const char *prototype, std::function<bk_NativeFunction> native);
    void AddGlobal(const char *name, const bk_TypeInfo *type, bk_PrimitiveValue value, bool mut = false);
};

}
