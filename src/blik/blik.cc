// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"
#include "parser.hh"
#include "run.hh"

namespace RG {

int RunBlik(int argc, char **argv)
{
    if (argc < 2) {
        PrintLn(stderr, "Usage: blik <file> ...");
        return 1;
    }

    for (Size i = 1; i < argc; i++) {
        HeapArray<char> code;
        if (ReadFile(argv[i], Megabytes(8), &code) < 0)
            return 1;

        TokenSet token_set;
        HeapArray<Instruction> ir;
        if (!Tokenize(code, argv[i], &token_set))
            return 1;
        if (!Parse(token_set.tokens, argv[i], &ir))
            return 1;

        Run(ir);
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunBlik(argc, argv); }
