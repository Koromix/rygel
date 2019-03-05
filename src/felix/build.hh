// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "toolchain.hh"
#include "target.hh"

struct BuildCommand {
    const char *type;

    const char *dest_filename;
    const char *cmd;

    bool sync_after;
};

struct BuildSet {
    HeapArray<BuildCommand> commands;

    BlockAllocator str_alloc;
};

class BuildSetBuilder {
    const Toolchain *toolchain;
    BuildMode build_mode;
    const char *output_directory; // TODO: Get rid of output_directory here

    HeapArray<BuildCommand> pch_commands;
    HeapArray<BuildCommand> obj_commands;
    HeapArray<BuildCommand> link_commands;
    BlockAllocator str_alloc;

    HashSet<const char *> output_set;

public:
    BuildSetBuilder(const Toolchain *toolchain, BuildMode build_mode,
                    const char *output_directory)
        : toolchain(toolchain), build_mode(build_mode), output_directory(output_directory) {}

    bool AppendTargetCommands(const TargetData &target);

    void Finish(BuildSet *out_set);

private:
    bool NeedsRebuild(const char *dest_filename, Span<const char *const> src_filenames);
};

bool RunBuildCommands(Span<const BuildCommand> commands, bool verbose);
