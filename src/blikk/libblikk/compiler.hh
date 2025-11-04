// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
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
