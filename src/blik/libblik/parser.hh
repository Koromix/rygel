// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

class ParserImpl;
struct TokenizedFile;

struct ParseReport {
    bool unexpected_eof;
    int depth;
};

class Parser {
    RG_DELETE_COPY(Parser)

    ParserImpl *impl;

public:
    Parser(Program *out_program);
    ~Parser();

    bool Parse(const TokenizedFile &file, ParseReport *out_report = nullptr);

    void AddFunction(const char *signature, std::function<NativeFunction> native);
};

bool Parse(const TokenizedFile &file, Program *out_program);

}
