// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/json.hh"
#include "ports.hh"

namespace RG {

extern "C" const RG::AssetInfo *const pack_asset_ports_pk_js;

static std::mutex js_mutex;
static std::condition_variable js_cv;
static ScriptPort js_ports[16];
static LocalArray<ScriptPort *, 16> js_idle_ports;

class JsonToQuickJS {
    RG_DELETE_COPY(JsonToQuickJS)

    enum class State {
        Start,
        Object,
        Value,
        Array,
        Done
    };

    JSContext *ctx;
    JSValue obj;

    State state = State::Start;

    JSAtom obj_prop;
    JSValue array;
    uint32_t array_len;

public:
    JsonToQuickJS(JSContext *ctx, JSValue obj) : ctx(ctx), obj(obj) {}

    ~JsonToQuickJS()
    {
        if (state == State::Value) {
            JS_FreeAtom(ctx, obj_prop);
        }
    }

    bool StartObject()
    {
        if (state == State::Start) {
            state = State::Object;
            return true;
        } else {
            LogError("Unexpected object");
            return false;
        }
    }
    bool EndObject(Size)
    {
        if (state == State::Object) {
            state = State::Done;
            return true;
        } else {
            LogError("Unexpected end of object");
            return false;
        }
    }

    bool StartArray()
    {
        if (state == State::Value) {
            array = JS_NewArray(ctx);
            HandleValue(array);
            array_len = 0;

            state = State::Array;
            return true;
        } else {
            LogError("Unexpected array");
            return false;
        }
    }
    bool EndArray(Size)
    {
        if (state == State::Array) {
            state = State::Object;
            return true;
        } else {
            LogError("Unexpected end of array");
            return false;
        }
    }

    bool Key(const char *key, Size, bool)
    {
        if (state == State::Object) {
            obj_prop = JS_NewAtom(ctx, key);
            state = State::Value;

            return true;
        } else {
            LogError("Unexpected key");
            return false;
        }
    }

    bool Null() { return HandleValue(JS_NULL); }
    bool Bool(bool b) { return HandleValue(b ? JS_TRUE : JS_FALSE); }
    bool Int(int i) { return HandleValue(JS_NewInt32(ctx, i)); }
    bool Uint(unsigned int u) { return HandleValue(JS_NewInt64(ctx, u)); }
    bool Int64(int64_t i) { return HandleValue(JS_NewBigInt64(ctx, i)); }
    bool Uint64(uint64_t u) { return HandleValue(JS_NewBigUint64(ctx, u)); }
    bool Double(double d) { return HandleValue(JS_NewFloat64(ctx, d)); }
    bool String(const char *str, Size len, bool)
        { return HandleValue(JS_NewStringLen(ctx, str, (size_t)len)); }

    bool RawNumber(const char *, Size, bool) { RG_UNREACHABLE(); }

private:
    bool HandleValue(JSValue value)
    {
        if (state == State::Value) {
            JS_SetProperty(ctx, obj, obj_prop, value);
            JS_FreeAtom(ctx, obj_prop);

            state = State::Object;
            return true;
        } else if (state == State::Array) {
            JS_SetPropertyUint32(ctx, array, array_len++, value);
            return true;
        } else {
            LogError("Unexpected value");
            JS_FreeValue(ctx, value);

            return false;
        }
    }
};

// This function does not try to deal with null/undefined values
static int ConsumeValueInt(JSContext *ctx, JSValue value)
{
    RG_DEFER { JS_FreeValue(ctx, value); };
    return JS_VALUE_GET_INT(value);
}

// Returns invalid Span (with ptr set to nullptr) if value is null/undefined
static Span<const char> ConsumeValueStr(JSContext *ctx, JSValue value)
{
    RG_DEFER { JS_FreeValue(ctx, value); };

    if (!JS_IsNull(value) && !JS_IsUndefined(value)) {
        Span<const char> str;
        str.ptr = JS_ToCStringLen(ctx, (size_t *)&str.len, value);
        return str;
    } else {
        return {};
    }
}

ScriptPort::~ScriptPort()
{
    if (ctx) {
        JS_FreeValue(ctx, validate_func);
        JS_FreeContext(ctx);
    }
    if (rt) {
        JS_FreeRuntime(rt);
    }
}

bool ScriptPort::ParseValues(StreamReader *st, ScriptHandle *out_handle)
{
    // Reinitialize (just in case)
    out_handle->~ScriptHandle();

    JSValue values = JS_NewObject(ctx);
    RG_DEFER_N(out_guard) { JS_FreeValue(ctx, values); };

    JsonToQuickJS converter(ctx, values);
    if (!json_Parse(st, &converter))
        return false;

    out_handle->ctx = ctx;
    out_handle->value = values;

    out_guard.Disable();
    return true;
}

// XXX: Detect errors (such as allocation failures) in calls to QuickJS
bool ScriptPort::RunRecord(Span<const char> script, const ScriptHandle &values, ScriptRecord *out_record)
{
    // Reinitialize (just in case)
    out_record->~ScriptRecord();

    JSValue args[2] = {
        JS_NewStringLen(ctx, script.ptr, script.len),
        JS_DupValue(ctx, values.value)
    };
    RG_DEFER {
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
    };

    JSValue ret = JS_Call(ctx, validate_func, JS_UNDEFINED, RG_LEN(args), args);
    if (JS_IsException(ret)) {
        const char *msg = JS_ToCString(ctx, JS_GetException(ctx));
        RG_DEFER { JS_FreeCString(ctx, msg); };

        LogError("JS: %1", msg);
        return false;
    }
    RG_DEFER { JS_FreeValue(ctx, ret); };

    // Record values (as JSON string) and errors
    out_record->json = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, ret, "json"));
    out_record->errors = ConsumeValueInt(ctx, JS_GetPropertyStr(ctx, ret, "errors"));

    // Variables
    {
        JSValue variables = JS_GetPropertyStr(ctx, ret, "variables");
        RG_DEFER { JS_FreeValue(ctx, variables); };

        int length = ConsumeValueInt(ctx, JS_GetPropertyStr(ctx, variables, "length"));

        for (int i = 0; i < length; i++) {
            const char *key = ConsumeValueStr(ctx, JS_GetPropertyUint32(ctx, variables, i)).ptr;
            out_record->variables.Append(key);
        }
    }

    return true;
}

void InitPorts()
{
    // QuickJS requires NUL termination, so we need to make a copy anyway
    HeapArray<char> code;
    {
        StreamReader st(pack_asset_ports_pk_js->data, nullptr, pack_asset_ports_pk_js->compression_type);

        Size read_len = st.ReadAll(Megabytes(1), &code);
        RG_ASSERT(read_len >= 0);

        code.Grow(1);
        code.ptr[code.len] = 0;
    }

    for (Size i = 0; i < RG_LEN(js_ports); i++) {
        ScriptPort *port = &js_ports[i];

        port->rt = JS_NewRuntime();
        port->ctx = JS_NewContext(port->rt);

        JSValue ret = JS_Eval(port->ctx, code.ptr, code.len, "ports.pk.js", JS_EVAL_TYPE_GLOBAL);
        RG_ASSERT(!JS_IsException(ret));

        JSValue global = JS_GetGlobalObject(port->ctx);
        JSValue server = JS_GetPropertyStr(port->ctx, global, "server");
        RG_DEFER {
            JS_FreeValue(port->ctx, server);
            JS_FreeValue(port->ctx, global);
        };

        port->validate_func = JS_GetPropertyStr(port->ctx, server, "validateRecord");

        js_idle_ports.Append(port);
    }
}

ScriptPort *LockPort()
{
    std::unique_lock<std::mutex> lock(js_mutex);

    while (!js_idle_ports.len) {
        js_cv.wait(lock);
    }

    ScriptPort *port = js_idle_ports[js_idle_ports.len - 1];
    js_idle_ports.RemoveLast(1);

    return port;
}

void UnlockPort(ScriptPort *port)
{
    std::unique_lock<std::mutex> lock(js_mutex);

    js_idle_ports.Append(port);

    js_cv.notify_one();
}

}
