// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

class ParserImpl;
struct TokenizedFile;

class Parser {
    ParserImpl *impl;

public:
    Parser(Program *out_program);
    ~Parser();

    Parser(const Parser &) = delete;
    Parser &operator=(const Parser &) = delete;

    bool Parse(const TokenizedFile &file);

    void AddFunction(const char *signature, NativeFunction *native);
};

bool Parse(const TokenizedFile &file, Program *out_program);

}
