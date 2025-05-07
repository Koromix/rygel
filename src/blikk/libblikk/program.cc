// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "program.hh"

namespace RG {

static bk_TypeInfo BaseTypes[] = {
    {"Null", bk_PrimitiveKind::Null, true, 0},
    {"Bool", bk_PrimitiveKind::Boolean, true, 1},
    {"Int", bk_PrimitiveKind::Integer, true, 1},
    {"Float", bk_PrimitiveKind::Float, true, 1},
    {"String", bk_PrimitiveKind::String, true, 1},
    {"Type", bk_PrimitiveKind::Type, false, 1}
};

Span<const bk_TypeInfo> bk_BaseTypes = BaseTypes;
const bk_TypeInfo *bk_NullType = &BaseTypes[0];
const bk_TypeInfo *bk_BoolType = &BaseTypes[1];
const bk_TypeInfo *bk_IntType = &BaseTypes[2];
const bk_TypeInfo *bk_FloatType = &BaseTypes[3];
const bk_TypeInfo *bk_StringType = &BaseTypes[4];
const bk_TypeInfo *bk_TypeType = &BaseTypes[5];

const char *bk_Program::LocateInstruction(const bk_FunctionInfo *func, Size pc, int32_t *out_line) const
{
    const bk_SourceMap *src;

    if (func) {
        src = &func->src;
    } else {
        if (!sources.len)
            return nullptr;

        src = std::upper_bound(sources.begin(), sources.end(), pc,
                               [](Size pc, const bk_SourceMap &src) { return pc < src.lines[0].addr; }) - 1;
        if (src < sources.ptr)
            return nullptr;
    }

    const bk_SourceMap::Line *line = std::upper_bound(src->lines.begin(), src->lines.end(), pc,
                                                       [](Size pc, const bk_SourceMap::Line &line) { return pc < line.addr; }) - 1;
    RG_ASSERT(line);

    if (out_line) {
        *out_line = line->line;
    }
    return src->filename;
}

}
