// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"

namespace RG {

int RunBuild(Span<const char *> arguments);
int RunPack(Span<const char *> arguments);

int Main(int argc, char **argv)
{
    // Handle help and version arguments
    if (argc >= 2) {
        if (TestStr(argv[1], "--help") || TestStr(argv[1], "help")) {
            if (argc >= 3 && argv[2][0] != '-') {
                argv[1] = argv[2];
                argv[2] = const_cast<char *>("--help");
            } else {
                const char *args[] = {"--help"};
                return RunBuild(args);
            }
        } else if (TestStr(argv[1], "--version")) {
            PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
            return 0;
        }
    }

    int (*cmd_func)(Span<const char *> arguments);
    Span<const char *> arguments;
    if (argc >= 2) {
        const char *cmd = argv[1];
        if (TestStr(cmd, "build")) {
            cmd_func = RunBuild;
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        } else if (TestStr(cmd, "pack")) {
            cmd_func = RunPack;
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        } else {
            cmd_func = RunBuild;
            arguments = MakeSpan((const char **)argv + 1, argc - 1);
        }
    } else {
        cmd_func = RunBuild;
        arguments = {};
    }

    return cmd_func(arguments);
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
