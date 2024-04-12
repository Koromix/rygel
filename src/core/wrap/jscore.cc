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

#include "src/core/base/base.hh"
#include "jscore.hh"

namespace RG {

void js_ExposeFunction(JSContextRef ctx, JSObjectRef obj, const char *name, JSObjectCallAsFunctionCallback func)
{
    js_AutoString key(name);

    JSValueRef value = JSObjectMakeFunctionWithCallback(ctx, key, func);
    JSObjectSetProperty(ctx, obj, key, value, kJSPropertyAttributeNone, nullptr);
}

bool js_PrintValue(JSContextRef ctx, JSValueRef value, JSValueRef *ex, StreamWriter *st)
{
    JSStringRef str = nullptr;

    if (JSValueIsString(ctx, value)) {
        str = (JSStringRef)value;
        JSStringRetain(str);
    } else {
        str = JSValueToStringCopy(ctx, value, ex);
        if (!str)
            return false;
    }
    RG_DEFER { JSStringRelease(str); };

    Size max = JSStringGetMaximumUTF8CStringSize(str);
    Span<char> buf = AllocateSpan<char>(nullptr, max);
    RG_DEFER { ReleaseSpan(nullptr, buf); };

    Size len = JSStringGetUTF8CString(str, buf.ptr, buf.len) - 1;
    RG_ASSERT(len >= 0);

    st->Write(buf.Take(0, len));

    return true;
}

}
