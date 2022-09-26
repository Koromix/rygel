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

enum class pwd_GenerateFlag {
    Uppers = 1 << 0,
    Lowers = 1 << 1,
    Numbers = 1 << 2,
    Specials = 1 << 3,
    Ambiguous = 1 << 4,
    Check = 1 << 5
};

bool pwd_CheckPassword(Span<const char> password, Span<const char *const> blacklist = {});

bool pwd_GeneratePassword(unsigned int flags, Span<char> out_password);
static inline bool pwd_GeneratePassword(Span<char> out_password)
    { return pwd_GeneratePassword(UINT_MAX, out_password); }

}
