// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"

namespace K {

enum class sb_IsolationFlag {
    Filesystem = 1 << 0,
    Signals = 1 << 1,
    Syscalls = 1 << 2
};
static const char *const sb_IsolationFlagNames[] = {
    "Filesystem",
    "Signals",
    "Syscalls"
};

struct sb_RevealedPath {
    const char *path;
    bool readonly;
};

enum class sb_FilterAction {
    Allow,
    Log,
    Block,
    Trap,
    Kill
};

struct sb_SyscallFilter {
    const char *name;
    sb_FilterAction action;
};

class sb_SandboxBuilder {
    K_DELETE_COPY(sb_SandboxBuilder)

#if defined(__linux__)
    unsigned int isolation = 0;

    HeapArray<sb_RevealedPath> reveals;
    HeapArray<sb_SyscallFilter> filters;
#endif

    BlockAllocator str_alloc;

public:
    sb_SandboxBuilder() = default;

    bool Init(unsigned int flags = UINT_MAX);

    void RevealPaths(Span<const sb_RevealedPath> reveals);
    void RevealPaths(Span<const char *const> paths, bool readonly);

#if defined(__linux__)
    void FilterSyscalls(Span<const sb_SyscallFilter> filters);
#endif

    // If this fails, just exit; the process is probably in a half-sandboxed
    // irrecoverable state.
    bool Apply();
};

}
