// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if !defined(__linux__)

#include "src/core/base/base.hh"
#include "sandbox.hh"

namespace K {

bool sb_SandboxBuilder::Init(unsigned int)
{
    LogError("Sandbox mode is not supported on this platform");
    return false;
}

void sb_SandboxBuilder::RevealPaths(Span<const char *const>, bool)
{
    K_ASSERT(false);
}

bool sb_SandboxBuilder::Apply()
{
    K_ASSERT(false);
    return false;
}

}

#endif
