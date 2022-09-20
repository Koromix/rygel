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

struct kt_ID {
    uint8_t hash[32];

    operator FmtArg() const { return FmtSpan(hash, FmtType::Hexadecimal, "").Pad0(-2); }
};

static inline void kt_FormatID(const kt_ID &id, char out_hex[65])
{
    Fmt(MakeSpan(out_hex, 65), "%1", FmtSpan(id.hash, FmtType::Hexadecimal, "").Pad0(-2));
}
static inline FmtArg kt_FormatID(const kt_ID &id)
{
    return (FmtArg)id;
}

bool kt_ParseID(const char *str, kt_ID *out_id);

}
