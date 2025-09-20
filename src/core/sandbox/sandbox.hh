// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "src/core/base/base.hh"

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
    struct RevealedPath {
        const char *path;
        bool readonly;
    };

    unsigned int isolation = 0;

    HeapArray<RevealedPath> reveals;
    HeapArray<const char *> masks;

    HeapArray<sb_SyscallFilter> filters;
#endif

    BlockAllocator str_alloc;

public:
    sb_SandboxBuilder() = default;

    bool Init(unsigned int flags = UINT_MAX);

    void RevealPaths(Span<const char *const> paths, bool readonly);
    void MaskFiles(Span<const char *const> filenames);

#if defined(__linux__)
    void FilterSyscalls(Span<const sb_SyscallFilter> filters);
#endif

    // If this fails, just exit; the process is probably in a half-sandboxed
    // irrecoverable state.
    bool Apply();
};

}
