// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

class Parser;
struct TokenizedFile;

class Compiler {
    Parser *parser;

public:
    Compiler();
    ~Compiler();

    Compiler(const Compiler &) = delete;
    Compiler &operator=(const Compiler &) = delete;

    bool Compile(const TokenizedFile &file);
    void AddFunction(const char *signature, NativeFunction *native);

    void Finish(Program *out_program);
};

bool Compile(const TokenizedFile &file, Program *out_program);

}
