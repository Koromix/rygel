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

#include "src/core/libcc/libcc.hh"
#include "types.hh"

namespace RG {

static inline int ParseHexadecimalChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

bool rk_ParseHash(const char *str, rk_Hash *out_hash)
{
    for (Size i = 0, j = 0; str[j]; i++, j += 2) {
        int high = ParseHexadecimalChar(str[j]);
        int low = (high >= 0) ? ParseHexadecimalChar(str[j + 1]) : -1;

        if (low < 0) {
            LogError("Malformed hash string '%1'", str);
            return false;
        }

        out_hash->hash[i] = (uint8_t)((high << 4) | low);
    }

    return true;
}

}
