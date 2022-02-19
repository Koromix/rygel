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

#include "src/core/libcc/libcc.hh"

namespace RG {

enum class sb_IsolationFlag {
    Network = 1 << 0
};

enum class sb_FilterAction {
    Allow,
    Block,
    Trap,
    Kill
};

struct sb_FilterItem {
    const char *name;
    sb_FilterAction action;
};

bool sb_IsSandboxSupported();

class sb_SandboxBuilder final {
    RG_DELETE_COPY(sb_SandboxBuilder)

#ifdef __linux__
    struct BindMount {
        const char *src;
        const char *dest;
        bool readonly;
    };

    unsigned int isolation_flags = 0;

    HeapArray<BindMount> mounts;
    HeapArray<const char *> masked_filenames;

    bool filter_syscalls = false;
    sb_FilterAction default_action;
    HeapArray<sb_FilterItem> filter_items;
#endif

    BlockAllocator str_alloc;

public:
    sb_SandboxBuilder() {};

    void SetIsolationFlags(unsigned int flags);

    void RevealPaths(Span<const char *const> paths, bool readonly);
    void MaskFiles(Span<const char *const> filenames);

#ifdef __linux__
    void MountPath(const char *src, const char *dest, bool readonly);
    void FilterSyscalls(sb_FilterAction default_action, Span<const sb_FilterItem> items = {});
    void FilterSyscalls(Span<const sb_FilterItem> items);
#endif

    // If this fails, just exit; the process is probably in a half-sandboxed
    // irrecoverable state.
    bool Apply();
};

}
