// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "jscore.hh"

namespace K {

void js_ExposeFunction(JSContextRef ctx, JSObjectRef obj, const char *name, JSObjectCallAsFunctionCallback func)
{
    js_AutoString key(name);

    JSValueRef value = JSObjectMakeFunctionWithCallback(ctx, key, func);
    JSObjectSetProperty(ctx, obj, key, value, kJSPropertyAttributeNone, nullptr);
}

Span<const char> js_ReadString(JSContextRef, JSStringRef str, Allocator *alloc)
{
    K_ASSERT(alloc);

    Size max = JSStringGetMaximumUTF8CStringSize(str);
    Span<char> buf = AllocateSpan<char>(alloc, max);

    buf.len = JSStringGetUTF8CString(str, buf.ptr, buf.len) - 1;
    K_ASSERT(buf.len >= 0);

    return buf;
}

Span<const char> js_ReadString(JSContextRef ctx, JSValueRef value, Allocator *alloc)
{
    K_ASSERT(JSValueIsString(ctx, value));

    JSStringRef str = JSValueToStringCopy(ctx, value, nullptr);
    if (!str)
        return false;
    K_DEFER { JSStringRelease(str); };

    return js_ReadString(ctx, str, alloc);
}

bool js_PrintValue(JSContextRef ctx, JSValueRef value, JSValueRef *ex, StreamWriter *st)
{
    JSStringRef str = JSValueToStringCopy(ctx, value, ex);
    if (!str)
        return false;
    K_DEFER { JSStringRelease(str); };

    Size max = JSStringGetMaximumUTF8CStringSize(str);
    Span<char> buf = AllocateSpan<char>(nullptr, max);
    K_DEFER { ReleaseSpan(nullptr, buf); };

    Size len = JSStringGetUTF8CString(str, buf.ptr, buf.len) - 1;
    K_ASSERT(len >= 0);

    st->Write(buf.Take(0, len));

    return true;
}

}
