// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __linux__

#include "../libcc/libcc.hh"
#include "sandbox.hh"

namespace RG {

bool sb_IsSandboxSupported()
{
    return false;
}

void sb_SandboxBuilder::IsolateProcess()
{
    RG_ASSERT(false);
}

void sb_SandboxBuilder::RevealPath(const char *, bool)
{
    RG_ASSERT(false);
}

void sb_SandboxBuilder::DropPrivileges()
{
    RG_ASSERT(false);
}

bool sb_SandboxBuilder::Apply()
{
    RG_ASSERT(false);
    return false;
}

}

#endif
