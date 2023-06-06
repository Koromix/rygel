// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "vendor/webkit/include/JavaScriptCore/JavaScript.h"

namespace RG {

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
