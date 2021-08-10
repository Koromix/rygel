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

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

static bk_TypeInfo BaseTypes[] = {
    {"Null", bk_PrimitiveKind::Null, 1},
    {"Bool", bk_PrimitiveKind::Boolean, 1},
    {"Int", bk_PrimitiveKind::Integer, 1},
    {"Float", bk_PrimitiveKind::Float, 1},
    {"String", bk_PrimitiveKind::String, 1},
    {"Type", bk_PrimitiveKind::Type, 1}
};

Span<const bk_TypeInfo> bk_BaseTypes = BaseTypes;
const bk_TypeInfo *bk_NullType = &BaseTypes[0];
const bk_TypeInfo *bk_BoolType = &BaseTypes[1];
const bk_TypeInfo *bk_IntType = &BaseTypes[2];
const bk_TypeInfo *bk_FloatType = &BaseTypes[3];
const bk_TypeInfo *bk_StringType = &BaseTypes[4];
const bk_TypeInfo *bk_TypeType = &BaseTypes[5];

const char *bk_Program::LocateInstruction(Size pc, int32_t *out_line) const
{
    const bk_SourceInfo *src = std::upper_bound(sources.begin(), sources.end(), pc,
                                                [](Size pc, const bk_SourceInfo &src) { return pc < src.lines[0].addr; }) - 1;
    if (src < sources.ptr)
        return nullptr;

    const bk_SourceInfo::Line *line = std::upper_bound(src->lines.begin(), src->lines.end(), pc,
                                                       [](Size pc, const bk_SourceInfo::Line &line) { return pc < line.addr; }) - 1;
    RG_ASSERT(line);

    if (out_line) {
        *out_line = line->line;
    }
    return src->filename;
}

}
