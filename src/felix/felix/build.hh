// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "compiler.hh"
#include "target.hh"

struct BuildCommand {
    const char *text;

    const char *dest_filename;
    const char *cmd;

    bool sync_after;
};

struct BuildSet {
    HeapArray<BuildCommand> commands;

    BlockAllocator str_alloc;
};

class BuildSetBuilder {
    const Compiler *compiler;
    BuildMode build_mode;

    BlockAllocator temp_alloc;

    HeapArray<BuildCommand> pch_commands;
    HeapArray<BuildCommand> obj_commands;
    HeapArray<BuildCommand> link_commands;
    BlockAllocator str_alloc;

    HashMap<const char *, int64_t> mtime_map;
    HashSet<const char *> output_set;

public:
    BuildSetBuilder(const Compiler *compiler, BuildMode build_mode)
        : compiler(compiler), build_mode(build_mode) {}

    bool AppendTargetCommands(const Target &target);

    void Finish(BuildSet *out_set);

private:
    bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames);
    int64_t GetFileModificationTime(const char *filename);
};

bool RunBuildCommands(Span<const BuildCommand> commands, bool verbose);
