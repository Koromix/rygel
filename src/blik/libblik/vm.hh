// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "error.hh"
#include "program.hh"

namespace RG {

class VirtualMachine {
    RG_DELETE_COPY(VirtualMachine)

    Span<const Instruction> ir;

    bool run;
    bool error;

public:
    const Program *const program;

    HeapArray<CallFrame> frames;
    HeapArray<Value> stack;

    VirtualMachine(const Program *const program);

    bool Run();

    void SetInterrupt() { run = false; }
    template <typename... Args>
    void FatalError(const char *fmt, Args... args)
    {
        ReportRuntimeError(*program, frames, fmt, args...);

        run = false;
        error = true;
    }

private:
    void DumpInstruction(Size pc) const;
};

bool Run(const Program &program);

}
