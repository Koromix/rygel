// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
PUSH_NO_WARNINGS()
#include "../../vendor/rapidjson/writer.h"
#include "../../vendor/rapidjson/error/en.h"
POP_NO_WARNINGS()

class json_StreamWriter {
    StreamWriter *st;
    LocalArray<uint8_t, 4096> buf;

public:
    typedef char Ch;

    json_StreamWriter(StreamWriter *st) : st(st) {}

    void Put(char c)
    {
        // TODO: Move the buffering to StreamWriter (when compression is enabled)
        buf.Append((uint8_t)c);
        if (buf.len == SIZE(buf.data)) {
            st->Write(buf);
            buf.Clear();
        }
    }
    void Flush()
    {
        st->Write(buf);
        buf.Clear();
    }
};

class json_Writer : public rapidjson::Writer<json_StreamWriter> {
    json_StreamWriter writer;

public:
    json_Writer(StreamWriter *st) : rapidjson::Writer<json_StreamWriter>(writer), writer(st) {}

    void Flush() { writer.Flush(); }
};
