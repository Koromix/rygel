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

enum class sec_FilterAction {
    Allow,
    Block,
    Trap,
    Kill
};

struct sec_FilterItem {
    const char *name;
    sec_FilterAction action;
};

bool sec_IsSandboxSupported();

class sec_SandboxBuilder final {
    RG_DELETE_COPY(sec_SandboxBuilder)

#ifdef __linux__
    struct BindMount {
        const char *src;
        const char *dest;
        bool readonly;
    };

    HeapArray<BindMount> mounts;
    HeapArray<const char *> masked_filenames;
    bool filter_syscalls = false;
    sec_FilterAction default_action;
    HeapArray<sec_FilterItem> filter_items;
#endif

    BlockAllocator str_alloc;

public:
    sec_SandboxBuilder() {};

    void RevealPaths(Span<const char *const> paths, bool readonly);
    void MaskFiles(Span<const char *const> filenames);

#ifdef __linux__
    void MountPath(const char *src, const char *dest, bool readonly);
    void FilterSyscalls(sec_FilterAction default_action, Span<const sec_FilterItem> items = {});
    void FilterSyscalls(Span<const sec_FilterItem> items);
#endif

    // If this fails, just exit; the process is probably in a half-sandboxed
    // irrecoverable state.
    bool Apply();
};

}
