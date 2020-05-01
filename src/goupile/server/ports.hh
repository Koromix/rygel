// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../../vendor/quickjs/quickjs.h"

namespace RG {

struct ScriptHandle {
    RG_DELETE_COPY(ScriptHandle)

    JSContext *ctx = nullptr;
    JSValue value = JS_UNDEFINED;

    ScriptHandle() = default;

    ~ScriptHandle()
    {
        if (ctx) {
            JS_FreeValue(ctx, value);
            ctx = nullptr;
        }
    }
};

struct ScriptRecord {
    RG_DELETE_COPY(ScriptRecord)

    JSContext *ctx = nullptr;

    Span<const char> json = {};
    HeapArray<const char *> variables;
    int errors;

    ScriptRecord() = default;

    ~ScriptRecord()
    {
        if (ctx) {
            JS_FreeCString(ctx, json.ptr);

            for (const char *variable: variables) {
                JS_FreeCString(ctx, variable);
            }

            ctx = nullptr;
        }
    }
};

class ScriptPort {
    RG_DELETE_COPY(ScriptPort)

    JSValue validate_func = JS_UNDEFINED;

public:
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;

    ScriptPort() = default;
    ~ScriptPort();

    bool ParseValues(StreamReader *st, ScriptHandle *out_handle);
    bool RunRecord(Span<const char> script, const ScriptHandle &values, ScriptRecord *out_record);

    friend void InitPorts();
};

void InitPorts();

ScriptPort *LockPort();
void UnlockPort(ScriptPort *port);

}
