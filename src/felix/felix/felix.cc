// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "build_command.hh"
#include "build_compiler.hh"
#include "build_target.hh"

int RunBuild(Span<const char *> arguments);
int RunPack(Span<const char *> arguments);

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: felix <command> [<args>]

Commands:
    build                        Build C and C++ projects (default)
    pack                         Pack assets to C source file and other formats)");
    };

    const char *cmd;
    Span<const char *> arguments;
    if (argc >= 2) {
        cmd = argv[1];
        arguments = MakeSpan((const char **)argv + 2, argc - 2);

        // Handle 'felix help [command]' and 'felix --help [command]' invocations
        if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
            if (arguments.len && arguments[0][0] != '-') {
                cmd = arguments[0];
                arguments[0] = "--help";
            } else {
                PrintUsage(stdout);
                return 0;
            }
        }
    } else {
        cmd = "build";
        arguments = {};
    }

    if (TestStr(cmd, "build")) {
        return RunBuild(arguments);
    } else if (TestStr(cmd, "pack")) {
        return RunPack(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}
