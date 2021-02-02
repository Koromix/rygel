// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
