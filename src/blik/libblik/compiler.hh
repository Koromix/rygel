// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

class Parser;
struct TokenizedFile;

struct CompileReport {
    bool unexpected_eof;
    int depth;
};

class Compiler {
    RG_DELETE_COPY(Compiler)

    Parser *parser;

public:
    Compiler(Program *out_program);
    ~Compiler();

    bool Compile(const TokenizedFile &file, CompileReport *out_report = nullptr);
    bool Compile(Span<const char> code, const char *filename, CompileReport *out_report = nullptr);

    void AddFunction(const char *signature, std::function<NativeFunction> native);
};

}
