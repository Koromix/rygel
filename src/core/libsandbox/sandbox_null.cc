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

#ifndef __linux__

#include "src/core/libcc/libcc.hh"
#include "sandbox.hh"

namespace RG {

bool sb_IsSandboxSupported()
{
    return false;
}

void sb_SandboxBuilder::SetIsolationFlags(unsigned int)
{
    RG_ASSERT(false);
}

void sb_SandboxBuilder::RevealPaths(Span<const char *const>, bool)
{
    RG_ASSERT(false);
}

void sb_SandboxBuilder::MaskFiles(Span<const char *const>)
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
