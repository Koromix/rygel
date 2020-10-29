// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../../vendor/quickjs/quickjs.h"

namespace RG {

struct Session;

struct ScriptRecord {
    RG_DELETE_COPY(ScriptRecord)

    JSContext *ctx = nullptr;
    const char *table = nullptr;
    const char *id = nullptr;
    const char *zone = nullptr;
    JSValue fragments = JS_UNDEFINED;

    ScriptRecord() = default;

    ~ScriptRecord()
    {
        if (ctx) {
            JS_FreeCString(ctx, table);
            JS_FreeCString(ctx, id);
            JS_FreeCString(ctx, zone);
            JS_FreeValue(ctx, fragments);
            ctx = nullptr;
        }
    }
};

struct ScriptFragment {
    RG_DELETE_COPY(ScriptFragment)

    struct Column {
        const char *key;
        const char *variable;
        const char *type;
        const char *prop;
    };

    JSContext *ctx = nullptr;
    const char *mtime = nullptr;
    int version;
    const char *page = nullptr;
    bool complete;
    Span<const char> json = {};
    int errors;
    HeapArray<Column> columns;

    ScriptFragment() = default;

    ~ScriptFragment()
    {
        if (ctx) {
            JS_FreeCString(ctx, mtime);
            JS_FreeCString(ctx, page);
            JS_FreeCString(ctx, json.ptr);

            for (const Column &col: columns) {
                JS_FreeCString(ctx, col.key);
                JS_FreeCString(ctx, col.variable);
                JS_FreeCString(ctx, col.type);
                JS_FreeCString(ctx, col.prop);
            }

            ctx = nullptr;
        }
    }
};

class ScriptPort {
    RG_DELETE_COPY(ScriptPort)

    JSValue profile_func = JS_UNDEFINED;
    JSValue validate_func = JS_UNDEFINED;

public:
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;

    ScriptPort() = default;
    ~ScriptPort();

    void ChangeProfile(const Session &session);

    bool ParseFragments(StreamReader *st, HeapArray<ScriptRecord> *out_handles);
    bool RunRecord(Span<const char> json, const ScriptRecord &handle,
                   HeapArray<ScriptFragment> *out_fragments, Span<const char> *out_json);

    friend void InitPorts();
};

void InitPorts();

ScriptPort *LockPort();
void UnlockPort(ScriptPort *port);

Size ConvertToJsName(const char *name, Span<char> out_buf);

}
