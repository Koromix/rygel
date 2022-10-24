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
#include "error.hh"
#include "program.hh"

namespace RG {

enum class bk_RunFlag {
    HideErrors = 1 << 0,
    Debug = 1 << 1
};

class bk_VirtualMachine {
    RG_DELETE_COPY(bk_VirtualMachine)

    bool run;
    bool error;
    bool report;

public:
    const bk_Program *const program;

    HeapArray<bk_CallFrame> frames;
    HeapArray<bk_PrimitiveValue> stack;

    bk_VirtualMachine(const bk_Program *const program);

    bool Run(unsigned int flags);

    void SetInterrupt() { run = false; }
    template <typename... Args>
    void FatalError(const char *fmt, Args... args)
    {
        if (report) {
            bk_ReportRuntimeError(*program, frames, fmt, args...);

            run = false;
            error = true;
        }
    }

private:
    void DumpInstruction(const bk_Instruction &inst, Size pc, Size bp) const;
};

bool bk_Run(const bk_Program &program, unsigned int flags = 0);

}
