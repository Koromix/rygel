// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "misc.hh"

namespace RG {

// This is used for static strings (e.g. permission names), and the Span<char>
// output buffer will abort debug builds on out-of-bounds access.
Size ConvertToJsName(const char *name, Span<char> out_buf)
{
    if (name[0]) {
        out_buf[0] = LowerAscii(name[0]);

        Size j = 1;
        for (Size i = 1; name[i]; i++) {
            if (name[i] >= 'A' && name[i] <= 'Z') {
                out_buf[j++] = '_';
                out_buf[j++] = LowerAscii(name[i]);
            } else {
                out_buf[j++] = name[i];
            }
        }
        out_buf[j] = 0;

        return j;
    } else {
        out_buf[0] = 0;
        return 0;
    }
}

}
