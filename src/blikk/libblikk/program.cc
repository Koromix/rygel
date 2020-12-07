// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

static bk_TypeInfo BaseTypes[] = {
    {"Null", bk_PrimitiveKind::Null},
    {"Bool", bk_PrimitiveKind::Boolean},
    {"Int", bk_PrimitiveKind::Integer},
    {"Float", bk_PrimitiveKind::Float},
    {"String", bk_PrimitiveKind::String},
    {"Type", bk_PrimitiveKind::Type}
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
