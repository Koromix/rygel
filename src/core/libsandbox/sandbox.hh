// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

namespace RG {

enum class sb_SyscallAction {
    Allow,
    Log,
    Block,
    Trap,
    Kill
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

    int unshare_flags = 0;
    HeapArray<BindMount> mounts;

    void *seccomp_ctx = nullptr; // scmp_filter_ctx
    uint32_t seccomp_kill_action;
    HashSet<int> filtered_syscalls;
    sb_SyscallAction default_action;
#endif

    BlockAllocator alloc;

public:
    sb_SandboxBuilder() {};
    ~sb_SandboxBuilder();

    void IsolateProcess();
    void MountPath(const char *src, const char *dest, bool readonly);

    bool InitSyscallFilter(sb_SyscallAction default_action);
    bool FilterSyscalls(sb_SyscallAction action, Span<const char *const> names);

    bool Apply();

private:
    uint32_t TranslateAction(sb_SyscallAction action) const;
};

}
