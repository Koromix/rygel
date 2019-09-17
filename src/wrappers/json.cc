// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "json.hh"

namespace RG {

json_StreamReader::json_StreamReader(StreamReader *st)
    : st(st)
{
    ReadByte();
}

char json_StreamReader::Take()
{
    char c = buf[buf_offset];
    if (c == '\n') {
        line_number++;
        line_offset = 1;
    } else {
        line_offset++;
    }
    ReadByte();
    return c;
}

void json_StreamReader::ReadByte()
{
    if (++buf_offset >= buf.len) {
        file_offset += buf.len;
        buf.len = st->Read(buf.data);
        buf_offset = 0;

        if (buf.len <= 0) {
            buf.len = 1;
            buf[0] = 0;
        }
    }
}

void json_StreamWriter::Put(char c)
{
    // TODO: Move the buffering to StreamWriter (when compression is enabled)
    buf.Append((uint8_t)c);

    if (buf.len == RG_SIZE(buf.data)) {
        st->Write(buf);
        buf.Clear();
    }
}

void json_StreamWriter::Flush()
{
    st->Write(buf);
    buf.Clear();
}

}
