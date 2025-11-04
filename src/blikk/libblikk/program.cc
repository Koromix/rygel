// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "program.hh"

namespace K {

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
    K_ASSERT(line);

    if (out_line) {
        *out_line = line->line;
    }
    return src->filename;
}

}
