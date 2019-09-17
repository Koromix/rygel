// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
RG_PUSH_NO_WARNINGS()
#include "../../vendor/rapidjson/reader.h"
#include "../../vendor/rapidjson/writer.h"
#include "../../vendor/rapidjson/error/en.h"
RG_POP_NO_WARNINGS()

namespace RG {

class json_StreamReader {
    StreamReader *st;

    LocalArray<uint8_t, 4096> buf;
    Size buf_offset = 0;
    Size file_offset = 0;

    Size line_number = 1;
    Size line_offset = 1;

public:
    typedef char Ch;

    json_StreamReader(StreamReader *st);

    char Peek() const { return buf[buf_offset]; }
    char Take();
    size_t Tell() const { return (size_t)(file_offset + buf_offset); }

    // Not implemented
    void Put(char) {}
    void Flush() {}
    char *PutBegin() { return 0; }
    Size PutEnd(char *) { return 0; }

    Size GetLineNumber() const { return line_number; }
    Size GetLineOffset() const { return line_offset; }

private:
    void ReadByte();
};

template <typename Handler>
bool json_Parse(StreamReader *st, Handler *handler)
{
    json_StreamReader json_reader(st);
    rapidjson::Reader parser;

    PushLogHandler([&](LogLevel level, const char *ctx, const char *msg) {
        StartConsoleLog(level);
        Print(stderr, "%1%2(%3:%4): %5", ctx, st->GetFileName(), json_reader.GetLineNumber(),
                                         json_reader.GetLineOffset(), msg);
        EndConsoleLog();
    });
    RG_DEFER { PopLogHandler(); };

    rapidjson::ParseErrorCode err = parser.Parse(json_reader, *handler).Code();
    if (err != rapidjson::kParseErrorNone) {
        // Parse error is likely after I/O error (missing token, etc.) but it's irrelevant,
        // the I/O error has already been issued. So don't log it.
        if (st->IsValid() && err != rapidjson::kParseErrorTermination) {
            LogError("%1", GetParseError_En(err));
        }

        return false;
    }

    return true;
}

class json_StreamWriter {
    StreamWriter *st;
    LocalArray<uint8_t, 4096> buf;

public:
    typedef char Ch;

    json_StreamWriter(StreamWriter *st) : st(st) {}

    void Put(char c);
    void Flush();
};

class json_Writer: public rapidjson::Writer<json_StreamWriter> {
    json_StreamWriter writer;

public:
    json_Writer(StreamWriter *st) : rapidjson::Writer<json_StreamWriter>(writer), writer(st) {}

    void Flush() { writer.Flush(); }
};

}
