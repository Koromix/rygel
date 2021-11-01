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

struct Vec2 {
    double x;
    double y;
};
struct Vec3 {
    double x;
    double y;
    double z;
};

struct PacketHeader {
    uint32_t crc32;
    uint16_t type; // MessageType
    uint16_t payload;
};
static_assert(sizeof(PacketHeader) == 8, "sizeof(PacketHeader) == 8");

#define MESSAGE(Name, Defn) struct Name ## Parameters Defn;
#include "protocol_inc.hh"

static const size_t PacketSizes[] = {
    #define MESSAGE(Name, Defn) sizeof(Name ## Parameters),
    #include "protocol_inc.hh"
};

enum class MessageType: uint16_t {
    #define MESSAGE(Name, Defn) Name,
    #include "protocol_inc.hh"
};
