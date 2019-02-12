// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
PUSH_NO_WARNINGS()
#include "../../lib/rapidjson/writer.h"
#include "../../lib/rapidjson/error/en.h"
POP_NO_WARNINGS()

class JsonStreamWriter {
    StreamWriter *st;
    LocalArray<uint8_t, 4096> buf;

public:
    typedef char Ch;

    JsonStreamWriter(StreamWriter *st) : st(st) {}

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

class JsonWriter : public rapidjson::Writer<JsonStreamWriter> {
    JsonStreamWriter writer;

public:
    JsonWriter(StreamWriter *st) : rapidjson::Writer<JsonStreamWriter>(writer), writer(st) {}

    void Flush() { writer.Flush(); }
};
