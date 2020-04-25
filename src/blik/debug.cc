// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "compiler.hh"
#include "debug.hh"
#include "types.hh"

namespace RG {

static void Decode1(const Program &program, Size pc, Size bp, HeapArray<FrameInfo> *out_frames)
{
    FrameInfo frame = {};

    frame.pc = pc;
    frame.bp = bp;
    if (bp) {
        auto func = std::lower_bound(program.functions.begin(), program.functions.end(), pc,
                                     [](const FunctionInfo &func, Size pc) { return func.inst_idx < pc; });
        --func;

        frame.func = &*func;
    }

    auto src = std::lower_bound(program.sources.begin(), program.sources.end(), pc,
                                [](const SourceInfo &src, Size pc) { return src.first_idx < pc; });
    src--;

    auto line = std::lower_bound(program.lines.begin() + src->line_idx, program.lines.end(), pc);
    line--;

    frame.filename = src->filename;
    frame.line = (int32_t)(line - (program.lines.ptr + src->line_idx) + 1);

    out_frames->Append(frame);
}

void DecodeFrames(const Program &program, Span<const Value> stack, Size pc, Size bp,
                  HeapArray<FrameInfo> *out_frames)
{
    // Walk up call frames
    if (bp) {
        Decode1(program, pc, bp, out_frames);

        for (;;) {
            pc = stack[bp - 2].i;
            bp = stack[bp - 1].i;

            if (!bp)
                break;

            Decode1(program, pc, bp, out_frames);
        }
    }

    // Outside funtion
    Decode1(program, pc, 0, out_frames);
}

}
