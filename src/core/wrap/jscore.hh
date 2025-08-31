// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

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
