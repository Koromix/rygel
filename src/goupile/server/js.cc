// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/json.hh"
#include "goupile.hh"
#include "instance.hh"
#include "js.hh"
#include "user.hh"

namespace RG {

static std::mutex js_mutex;
static std::condition_variable js_cv;
static ScriptPort js_ports[16];
static LocalArray<ScriptPort *, 16> js_idle_ports;

// These functions do not try to deal with null/undefined values
static bool ConsumeValueBool(JSContext *ctx, JSValue value)
{
    RG_DEFER { JS_FreeValue(ctx, value); };
    return JS_VALUE_GET_BOOL(value);
}
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
        JS_FreeValue(ctx, profile_func);
        JS_FreeValue(ctx, validate_func);
        JS_FreeContext(ctx);
    }
    if (rt) {
        JS_FreeRuntime(rt);
    }
}

ScriptPort *LockScriptPort()
{
    std::unique_lock<std::mutex> lock(js_mutex);

    while (!js_idle_ports.len) {
        js_cv.wait(lock);
    }

    ScriptPort *port = js_idle_ports[js_idle_ports.len - 1];
    js_idle_ports.RemoveLast(1);

    return port;
}

void ScriptPort::Unlock()
{
    std::unique_lock<std::mutex> lock(js_mutex);

    js_idle_ports.Append(this);

    js_cv.notify_one();
}

void ScriptPort::Setup(InstanceData *instance, const Session &session, const Token &token)
{
    JS_SetContextOpaque(ctx, instance);

    JSValue args[] = {
        JS_NewString(ctx, session.username),
        token.zone ? JS_NewString(ctx, token.zone) : JS_NULL
    };
    RG_DEFER {
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
    };

    JSValue ret = JS_Call(ctx, profile_func, JS_UNDEFINED, RG_LEN(args), args);
    RG_ASSERT(!JS_IsException(ret));
    RG_DEFER { JS_FreeValue(ctx, ret); };
}

static JSValue ReadFile(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    InstanceData *instance = (InstanceData *)JS_GetContextOpaque(ctx);

    const char *filename = JS_ToCString(ctx, argv[0]);
    if (!filename)
        return JS_EXCEPTION;
    RG_DEFER { JS_FreeCString(ctx, filename); };

    if (!StartsWith(filename, "/files/")) {
        JS_ThrowReferenceError(ctx, "Cannot read file outside '/files/'");
        return JS_EXCEPTION;
    }
    if (PathContainsDotDot(filename)) {
        JS_ThrowReferenceError(ctx, "Unsafe filename '%s'", filename);
        return JS_EXCEPTION;
    }

    sq_Statement stmt;
    if (!instance->db.Prepare(R"(SELECT compression, blob FROM fs_files
                                 WHERE path = ? AND sha256 IS NOT NULL;)", &stmt)) {
        JS_ThrowInternalError(ctx, "SQLite Error: %s", sqlite3_errmsg(instance->db));
        return JS_EXCEPTION;
    }
    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

    if (!stmt.Next()) {
        if (stmt.IsValid()) {
            JS_ThrowReferenceError(ctx, "Cannot load file '%s'", filename);
        } else {
            JS_ThrowInternalError(ctx, "SQLite Error: %s", sqlite3_errmsg(instance->db));
        }

        return JS_EXCEPTION;
    }

    CompressionType compression_type;
    {
        const char *str = (const char *)sqlite3_column_text(stmt, 0);
        if (!OptionToEnum(CompressionTypeNames, str, &compression_type)) {
            JS_ThrowInternalError(ctx, "Invalid compression type '%s'", str);
            return JS_EXCEPTION;
        }
    }

    Span<const uint8_t> blob = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 1),
                                        sqlite3_column_bytes(stmt, 1));

    if (compression_type == CompressionType::None) {
        return JS_NewStringLen(ctx, (const char *)blob.ptr, blob.len);
    } else {
        StreamReader reader(blob, filename, compression_type);

        HeapArray<uint8_t> buf;
        if (reader.ReadAll(instance->config.max_file_size, &buf) < 0) {
            JS_ThrowInternalError(ctx, "Failed to decompress '%s'", filename);
            return JS_EXCEPTION;
        }

        return JS_NewStringLen(ctx, (const char *)buf.ptr, buf.len);
    }
}

bool ScriptPort::ParseFragments(StreamReader *st, HeapArray<ScriptRecord> *out_handles)
{
    RG_DEFER_NC(out_guard, len = out_handles->len) { out_handles->RemoveFrom(len); };

    BlockAllocator temp_alloc;

    json_Parser parser(st, &temp_alloc);

    parser.ParseArray();
    while (parser.InArray()) {
        ScriptRecord *handle = out_handles->AppendDefault();

        handle->ctx = ctx;
        handle->fragments = JS_NewArray(ctx);

        const char *table = nullptr;
        const char *id = nullptr;
        const char *zone = nullptr;
        uint32_t fragments_len = 0;

        parser.ParseObject();
        while (parser.InObject()) {
            const char *key = "";
            parser.ParseKey(&key);

            if (TestStr(key, "table")) {
                parser.ParseString(&table);
            } else if (TestStr(key, "id")) {
                parser.ParseString(&id);
            } else if (TestStr(key, "zone")) {
                switch (parser.PeekToken()) {
                    case json_TokenType::Null: {
                        parser.ParseNull();
                        zone = nullptr;
                    } break;
                    case json_TokenType::String: { parser.ParseString(&zone); } break;

                    default: {
                        LogError("Unexpected token type '%1'", json_TokenTypeNames[(int)parser.PeekToken()]);
                        return false;
                    } break;
                }
            } else if (TestStr(key, "fragments")) {
                parser.ParseArray();
                while (parser.InArray()) {
                    const char *mtime = nullptr;
                    int64_t version = -1;
                    const char *page = nullptr;
                    bool deletion = false;
                    bool complete = false;

                    JSValue frag = JS_NewObject(ctx);
                    JSValue values = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, frag, "values", values);
                    JS_SetPropertyUint32(ctx, handle->fragments, fragments_len++, frag);

                    parser.ParseObject();
                    while (parser.InObject()) {
                        const char *key = "";
                        parser.ParseKey(&key);

                        if (TestStr(key, "mtime")) {
                            parser.ParseString(&mtime);
                        } else if (TestStr(key, "version")) {
                            parser.ParseInt(&version);
                        } else if (TestStr(key, "page")) {
                            if (parser.PeekToken() == json_TokenType::Null) {
                                parser.ParseNull();
                                deletion = true;
                            } else {
                                parser.ParseString(&page);
                                deletion = false;
                            }
                        } else if (TestStr(key, "complete")) {
                            parser.ParseBool(&complete);
                        } else if (TestStr(key, "values")) {
                            parser.ParseObject();
                            while (parser.InObject()) {
                                const char *obj_key = nullptr;
                                parser.ParseKey(&obj_key);

                                JSAtom obj_prop = JS_NewAtom(ctx, obj_key);
                                RG_DEFER { JS_FreeAtom(ctx, obj_prop); };

                                switch (parser.PeekToken()) {
                                    case json_TokenType::Null: {
                                        parser.ParseNull();
                                        JS_SetProperty(ctx, values, obj_prop, JS_NULL);
                                    } break;
                                    case json_TokenType::Bool: {
                                        bool b = false;
                                        parser.ParseBool(&b);

                                        JS_SetProperty(ctx, values, obj_prop, b ? JS_TRUE : JS_FALSE);
                                    } break;
                                    case json_TokenType::Integer: {
                                        int64_t i = 0;
                                        parser.ParseInt(&i);

                                        if (i >= INT_MIN || i <= INT_MAX) {
                                            JS_SetProperty(ctx, values, obj_prop, JS_NewInt32(ctx, (int32_t)i));
                                        } else {
                                            JS_SetProperty(ctx, values, obj_prop, JS_NewBigInt64(ctx, i));
                                        }
                                    } break;
                                    case json_TokenType::Double: {
                                        double d = 0.0;
                                        parser.ParseDouble(&d);

                                        JS_SetProperty(ctx, values, obj_prop, JS_NewFloat64(ctx, d));
                                    } break;
                                    case json_TokenType::String: {
                                        Span<const char> str;
                                        parser.ParseString(&str);

                                        JS_SetProperty(ctx, values, obj_prop, JS_NewStringLen(ctx, str.ptr, (size_t)str.len));
                                    } break;

                                    case json_TokenType::StartArray: {
                                        JSValue array = JS_NewArray(ctx);
                                        uint32_t len = 0;

                                        JS_SetProperty(ctx, values, obj_prop, array);

                                        parser.ParseArray();
                                        while (parser.InArray()) {
                                            switch (parser.PeekToken()) {
                                                case json_TokenType::Null: {
                                                    parser.ParseNull();
                                                    JS_SetPropertyUint32(ctx, array, len++, JS_NULL);
                                                } break;
                                                case json_TokenType::Bool: {
                                                    bool b = false;
                                                    parser.ParseBool(&b);

                                                    JS_SetPropertyUint32(ctx, array, len++, b ? JS_TRUE : JS_FALSE);
                                                } break;
                                                case json_TokenType::Integer: {
                                                    int64_t i = 0;
                                                    parser.ParseInt(&i);

                                                    if (i >= INT_MIN || i <= INT_MAX) {
                                                        JS_SetPropertyUint32(ctx, array, len++, JS_NewInt32(ctx, (int32_t)i));
                                                    } else {
                                                        JS_SetPropertyUint32(ctx, array, len++, JS_NewBigInt64(ctx, i));
                                                    }
                                                } break;
                                                case json_TokenType::Double: {
                                                    double d = 0.0;
                                                    parser.ParseDouble(&d);

                                                    JS_SetPropertyUint32(ctx, array, len++, JS_NewFloat64(ctx, d));
                                                } break;
                                                case json_TokenType::String: {
                                                    Span<const char> str;
                                                    parser.ParseString(&str);

                                                    JS_SetPropertyUint32(ctx, array, len++, JS_NewStringLen(ctx, str.ptr, (size_t)str.len));
                                                } break;

                                                default: {
                                                    LogError("Unexpected token type '%1'", json_TokenTypeNames[(int)parser.PeekToken()]);
                                                    return false;
                                                } break;
                                            }
                                        }
                                    } break;

                                    default: {
                                        LogError("Unexpected token type '%1'", json_TokenTypeNames[(int)parser.PeekToken()]);
                                        return false;
                                    } break;
                                }
                            }
                        } else {
                            LogError("Unknown key '%1' in fragment object", key);
                            return false;
                        }
                    }
                    if (!parser.IsValid())
                        return false;

                    if (((!page || !page[0]) && !deletion) || !mtime || !mtime[0] || version < 0) {
                        LogError("Missing mtime, version or page attribute");
                        return false;
                    }

                    JS_SetPropertyStr(ctx, frag, "mtime", JS_NewString(ctx, mtime));
                    JS_SetPropertyStr(ctx, frag, "version", JS_NewInt64(ctx, version));
                    JS_SetPropertyStr(ctx, frag, "page", !deletion ? JS_NewString(ctx, page) : JS_NULL);
                    JS_SetPropertyStr(ctx, frag, "complete", complete ? JS_TRUE : JS_FALSE);
                }
            }
        }
        if (!parser.IsValid())
            return false;

        if (!table || !table[0] || !id || !id[0]) {
            LogError("Missing table or id attribute");
            return false;
        }
        if (zone && !zone[0]) {
            LogError("Zone attribute cannot be empty");
            return false;
        }

        handle->table = ConsumeValueStr(ctx, JS_NewString(ctx, table)).ptr;
        handle->id = ConsumeValueStr(ctx, JS_NewString(ctx, id)).ptr;
        handle->zone = zone ? ConsumeValueStr(ctx, JS_NewString(ctx, zone)).ptr : nullptr;
    }
    if (!parser.IsValid())
        return false;

    out_guard.Disable();
    return true;
}

// XXX: Detect errors (such as allocation failures) in calls to QuickJS
bool ScriptPort::RunRecord(Span<const char> json, const ScriptRecord &handle,
                           HeapArray<ScriptFragment> *out_fragments, Span<const char> *out_json)
{
    JSValue ret;
    {
        JSValue args[] = {
            JS_NewString(ctx, handle.table),
            JS_NewStringLen(ctx, json.ptr, (size_t)json.len),
            JS_DupValue(ctx, handle.fragments)
        };
        RG_DEFER {
            JS_FreeValue(ctx, args[0]);
            JS_FreeValue(ctx, args[1]);
            JS_FreeValue(ctx, args[2]);
        };

        ret = JS_Call(ctx, validate_func, JS_UNDEFINED, RG_LEN(args), args);
    }
    RG_DEFER { JS_FreeValue(ctx, ret); };
    if (JS_IsException(ret)) {
        const char *msg = ConsumeValueStr(ctx, JS_GetException(ctx)).ptr;
        RG_DEFER { JS_FreeCString(ctx, msg); };

        LogError("JS: %1", msg);
        return false;
    }

    JSValue fragments = JS_GetPropertyStr(ctx, ret, "fragments");
    int fragments_len = ConsumeValueInt(ctx, JS_GetPropertyStr(ctx, fragments, "length"));
    RG_DEFER { JS_FreeValue(ctx, fragments); };
    *out_json = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, ret, "json"));

    for (int i = 0; i< fragments_len; i++) {
        JSValue frag = JS_GetPropertyUint32(ctx, fragments, i);
        RG_DEFER { JS_FreeValue(ctx, frag); };

        ScriptFragment *frag2 = out_fragments->AppendDefault();
        frag2->ctx = ctx;

        frag2->mtime = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, frag, "mtime")).ptr;
        frag2->version = ConsumeValueInt(ctx, JS_GetPropertyStr(ctx, frag, "version"));
        frag2->page = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, frag, "page")).ptr;
        frag2->complete = ConsumeValueBool(ctx, JS_GetPropertyStr(ctx, frag, "complete"));
        frag2->json = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, frag, "json"));
        frag2->errors = ConsumeValueInt(ctx, JS_GetPropertyStr(ctx, frag, "errors"));

        JSValue columns = JS_GetPropertyStr(ctx, frag, "columns");
        RG_DEFER { JS_FreeValue(ctx, columns); };

        if (!JS_IsNull(columns) && !JS_IsUndefined(columns)) {
            int columns_len = ConsumeValueInt(ctx, JS_GetPropertyStr(ctx, columns, "length"));

            for (int j = 0; j < columns_len; j++) {
                JSValue col = JS_GetPropertyUint32(ctx, columns, j);
                RG_DEFER { JS_FreeValue(ctx, col); };

                ScriptFragment::Column *col2 = frag2->columns.AppendDefault();

                col2->key = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, col, "key")).ptr;
                col2->variable = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, col, "variable")).ptr;
                col2->type = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, col, "type")).ptr;
                col2->prop = ConsumeValueStr(ctx, JS_GetPropertyStr(ctx, col, "prop")).ptr;
            }
        }
    }

    return true;
}

void InitJS()
{
    const AssetInfo *asset = FindPackedAsset("server.pk.js");
    RG_ASSERT(asset);

    // QuickJS requires NUL termination, so we need to make a copy anyway
    HeapArray<char> code;
    {
        StreamReader st(asset->data, nullptr, asset->compression_type);

        Size read_len = st.ReadAll(Megabytes(1), &code);
        RG_ASSERT(read_len >= 0);

        code.Grow(1);
        code.ptr[code.len] = 0;
    }

    for (Size i = 0; i < RG_LEN(js_ports); i++) {
        ScriptPort *port = &js_ports[i];

        port->rt = JS_NewRuntime();
        port->ctx = JS_NewContext(port->rt);

        JSValue ret = JS_Eval(port->ctx, code.ptr, code.len, "server.pk.js", JS_EVAL_TYPE_GLOBAL);
        RG_ASSERT(!JS_IsException(ret));

        JSValue global = JS_GetGlobalObject(port->ctx);
        JSValue server = JS_GetPropertyStr(port->ctx, global, "server");
        RG_DEFER {
            JS_FreeValue(port->ctx, server);
            JS_FreeValue(port->ctx, global);
        };

        JS_SetPropertyStr(port->ctx, server, "readFile", JS_NewCFunction(port->ctx, ReadFile, "readFile", 1));

        port->profile_func = JS_GetPropertyStr(port->ctx, server, "changeProfile");
        port->validate_func = JS_GetPropertyStr(port->ctx, server, "validateFragments");

        js_idle_ports.Append(port);
    }
}

}
