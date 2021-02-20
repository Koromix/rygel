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

#include "../libcc/libcc.hh"

namespace RG {

enum class sb_FilterAction {
    Allow,
    Log,
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

    bool valid = true;

    bool unshare = false;
    HeapArray<BindMount> mounts;
    bool drop_caps = false;
    bool filter_syscalls = false;
    sb_FilterAction default_action;
    HeapArray<sb_FilterItem> filter_items;
#endif

    BlockAllocator str_alloc;

public:
    sb_SandboxBuilder() {};

    void DropPrivileges();
    void IsolateProcess();
    void RevealPath(const char *path, bool readonly);

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
