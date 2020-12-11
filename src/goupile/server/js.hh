// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../../vendor/quickjs/quickjs.h"

namespace RG {

struct ScriptRecord {
    JSContext *ctx = nullptr;
    const char *table = nullptr;
    const char *id = nullptr;
    const char *zone = nullptr;
    JSValue fragments = JS_UNDEFINED;
};

struct ScriptFragment {
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
};

class ScriptPort {
    RG_DELETE_COPY(ScriptPort)

    JSValue profile_func = JS_UNDEFINED;
    JSValue validate_func = JS_UNDEFINED;
    JSValue recompute_func = JS_UNDEFINED;

    HeapArray<JSValue> values;
    HeapArray<const char *> strings;

    bool locked = false;

public:
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;

    ScriptPort() = default;
    ~ScriptPort();

    void Unlock();
    void ReleaseAll();

    void Setup(InstanceHolder *instance, const char *username, const char *zone);

    bool ParseFragments(StreamReader *st, HeapArray<ScriptRecord> *out_handles);
    bool RunRecord(Span<const char> json, const ScriptRecord &handle,
                   HeapArray<ScriptFragment> *out_fragments, Span<const char> *out_json);

    bool Recompute(const char *table, Span<const char> json, const char *page,
                    ScriptFragment *out_fragment, Span<const char> *out_json);

private:
    JSValue StoreValue(JSValue value);

    bool ConsumeBool(JSValue value);
    int ConsumeInt(JSValue value);
    Span<const char> ConsumeStr(JSValue value);

    friend void InitJS();
    friend ScriptPort *LockScriptPort();
};

void InitJS();

ScriptPort *LockScriptPort();

}
