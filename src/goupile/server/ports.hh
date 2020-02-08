// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../../vendor/quickjs/quickjs.h"

namespace RG {

struct CheckedRecord {
    JSContext *ctx = nullptr;

    Span<const char> json = {};
    HeapArray<const char *> variables;
    int errors;

    ~CheckedRecord()
    {
        if (ctx) {
            JS_FreeCString(ctx, json.ptr);

            for (const char *variable: variables) {
                JS_FreeCString(ctx, variable);
            }
        }
    }
};

class ScriptPort {
    JSValue validate_func = JS_UNDEFINED;

public:
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;

    ~ScriptPort();

    bool ParseValues(StreamReader *st, JSValue *out_values);
    void FreeValues(JSValue values);

    bool RunRecord(Span<const char> script, JSValue values, CheckedRecord *out_record);

    friend void InitPorts();
};

void InitPorts();

ScriptPort *LockPort();
void UnlockPort(ScriptPort *port);

}
