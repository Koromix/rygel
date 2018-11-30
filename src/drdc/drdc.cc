// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"

bool RunMcoClassify(Span<const char *> arguments);
bool RunMcoDump(Span<const char *> arguments);
bool RunMcoList(Span<const char *> arguments);
bool RunMcoMap(Span<const char *> arguments);
bool RunMcoPack(Span<const char *> arguments);
bool RunMcoShow(Span<const char *> arguments);

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: drdc <command> [<args>]

Commands:
    mco_classify                 Classify MCO stays
    mco_dump                     Dump available MCO tables and lists
    mco_list                     Export MCO diagnosis and procedure lists
    mco_map                      Compute GHM accessibility constraints
    mco_pack                     Pack MCO stays for quicker loads
    mco_show                     Print information about individual MCO elements
                                 (diagnoses, procedures, GHM roots, etc.)
)");
    };

    LinkedAllocator temp_alloc;

    if (argc < 2) {
        PrintUsage(stderr);
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = "--help";
        } else {
            PrintUsage(stdout);
            return 0;
        }
    }

#define HANDLE_COMMAND(Cmd, Func) \
        do { \
            if (TestStr(cmd, STRINGIFY(Cmd))) { \
                return !Func(arguments); \
            } \
        } while (false)

    HANDLE_COMMAND(mco_classify, RunMcoClassify);
    HANDLE_COMMAND(mco_dump, RunMcoDump);
    HANDLE_COMMAND(mco_list, RunMcoList);
    HANDLE_COMMAND(mco_map, RunMcoMap);
    HANDLE_COMMAND(mco_pack, RunMcoPack);
    HANDLE_COMMAND(mco_show, RunMcoShow);

#undef HANDLE_COMMAND

    LogError("Unknown command '%1'", cmd);
    return 1;
}
