// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "compiler.hh"

namespace RG {

int RunBuild(Span<const char *> arguments);
int RunMacify(Span<const char *> arguments);
int RunEmbed(Span<const char *> arguments);

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

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
            PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
            PrintLn("Compiler: %1", FelixCompiler);
            PrintLn("Host: %1", HostPlatformNames[(int)NativePlatform]);
            PrintLn("Architecture: %1", HostArchitectureNames[(int)NativeArchitecture]);

            return 0;
        }
    }

    const char *cmd = nullptr;
    Span<const char *> arguments = {};

    if (argc >= 2) {
        cmd = argv[1];

        if (cmd[0] == '-') {
            cmd = "build";
            arguments = MakeSpan((const char **)argv + 1, argc - 1);
        } else {
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        }
    } else {
        cmd = "build";
    }

    if (TestStr(cmd, "build")) {
        return RunBuild(arguments);
#if defined(__APPLE__)
    } else if (TestStr(cmd, "macify")) {
        return RunMacify(arguments);
#endif
    } else if (TestStr(cmd, "embed")) {
        return RunEmbed(arguments);
    } else {
        arguments = MakeSpan((const char **)argv + 1, argc - 1);
        return RunBuild(arguments);
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
