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
    void AddGlobal(const char *name, const bk_TypeInfo *type, Span<const bk_PrimitiveValue> values, bool mut = false);
};

}
