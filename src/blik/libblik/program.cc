// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

const char *Program::LocateInstruction(Size pc, int32_t *out_line) const
{
    const SourceInfo *src = std::upper_bound(sources.begin(), sources.end(), pc,
                                             [](Size pc, const SourceInfo &src) { return pc < src.lines[0].first_idx; }) - 1;
    if (src < sources.ptr)
        return nullptr;

    const SourceInfo::Line *line = std::upper_bound(src->lines.begin(), src->lines.end(), pc,
                                                    [](Size pc, const SourceInfo::Line &line) { return pc < line.first_idx; }) - 1;
    RG_ASSERT(line);

    if (out_line) {
        *out_line = line->line;
    }
    return src->filename;
}

}
