// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "vendor/webkit/include/JavaScriptCore/JavaScript.h"

namespace K {

class js_AutoString {
    K_DELETE_COPY(js_AutoString)

    JSStringRef ref = nullptr;

public:
    js_AutoString() = default;
    js_AutoString(const char *str) { ref = JSStringCreateWithUTF8CString(str); }
    js_AutoString(Span<const char> str) { ref = JSStringCreateWithUTF8CStringWithLength(str.ptr, (size_t)str.len); }

    ~js_AutoString() { Reset(); }

    void Reset()
    {
        if (ref) {
            JSStringRelease(ref);
        }
        ref = nullptr;
    }
    void Reset(const char *str)
    {
        Reset();
        ref = JSStringCreateWithUTF8CString(str);
    }
    void Reset(Span<const char> str)
    {
        Reset();
        ref = JSStringCreateWithUTF8CStringWithLength(str.ptr, (size_t)str.len);
    }

    operator JSStringRef() const { return ref; }
};

static inline bool js_IsNullOrUndefined(JSContextRef ctx, JSValueRef value)
{
    return JSValueIsNull(ctx, value) || JSValueIsUndefined(ctx, value);
}

void js_ExposeFunction(JSContextRef ctx, JSObjectRef obj, const char *name, JSObjectCallAsFunctionCallback func);

Span<const char> js_ReadString(JSContextRef ctx, JSStringRef str, Allocator *alloc);
Span<const char> js_ReadString(JSContextRef ctx, JSValueRef value, Allocator *alloc);

bool js_PrintValue(JSContextRef ctx, JSValueRef value, JSValueRef *ex, StreamWriter *st);

}
