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

struct ScriptFragment {
    RG_DELETE_COPY(ScriptFragment)

    struct Column {
        const char *key;
        const char *prop;
    };

    JSContext *ctx = nullptr;
    const char *mtime = nullptr;
    const char *page = nullptr;
    Span<const char> values = {};
    int errors;
    HeapArray<Column> columns;

    ScriptFragment() = default;

    ~ScriptFragment()
    {
        if (ctx) {
            JS_FreeCString(ctx, mtime);
            JS_FreeCString(ctx, page);
            JS_FreeCString(ctx, values.ptr);
            for (const Column &col: columns) {
                JS_FreeCString(ctx, col.key);
                JS_FreeCString(ctx, col.prop);
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

    bool ParseFragments(StreamReader *st, ScriptHandle *out_handle);
    bool RunRecord(const ScriptHandle &handle, HeapArray<ScriptFragment> *out_fragments);

    friend void InitPorts();
};

void InitPorts();

ScriptPort *LockPort();
void UnlockPort(ScriptPort *port);

}
