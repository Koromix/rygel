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

#include "src/core/libcc/libcc.hh"
RG_PUSH_NO_WARNINGS
#define RAPIDJSON_NO_SIZETYPEDEFINE
namespace rapidjson { typedef RG::Size SizeType; }
#include "vendor/rapidjson/reader.h"
#include "vendor/rapidjson/writer.h"
#include "vendor/rapidjson/error/en.h"
RG_POP_NO_WARNINGS

namespace RG {

class json_StreamReader {
    RG_DELETE_COPY(json_StreamReader)

    StreamReader *st;

    LocalArray<uint8_t, 4096> buf;
    Size buf_offset = 0;
    Size file_offset = 0;

    int line_number = 1;
    int line_offset = 1;

public:
    typedef char Ch;

    json_StreamReader(StreamReader *st);

    bool IsValid() const { return st->IsValid(); }

    char Peek() const { return buf[buf_offset]; }
    char Take();
    size_t Tell() const { return (size_t)(file_offset + buf_offset); }

    // Not implemented
    void Put(char) {}
    void Flush() {}
    char *PutBegin() { return 0; }
    Size PutEnd(char *) { return 0; }

    const char *GetFileName() const { return st->GetFileName(); }
    int GetLineNumber() const { return line_number; }
    int GetLineOffset() const { return line_offset; }

private:
    void ReadByte();
};

enum class json_TokenType {
    Invalid,

    StartObject,
    EndObject,
    StartArray,
    EndArray,

    Null,
    Bool,
    Number,
    String,

    Key
};
static const char *const json_TokenTypeNames[] = {
    "Invalid",

    "Object",
    "End of object",
    "Array",
    "End of array",

    "Null",
    "Boolean",
    "Number",
    "String",

    "Key"
};

class json_Parser {
    RG_DELETE_COPY(json_Parser)

    struct Handler {
        Allocator *allocator;

        json_TokenType token = json_TokenType::Invalid;
        union {
            bool b;
            LocalArray<char, 128> num;
            Span<const char> str;
        } u = {};

        bool StartObject();
        bool EndObject(Size);
        bool StartArray();
        bool EndArray(Size);

        bool Null();
        bool Bool(bool b);
        bool Double(double) { RG_UNREACHABLE(); }
        bool Int(int) { RG_UNREACHABLE(); }
        bool Int64(int64_t) { RG_UNREACHABLE(); }
        bool Uint(unsigned int) { RG_UNREACHABLE(); }
        bool Uint64(uint64_t) { RG_UNREACHABLE(); }
        bool RawNumber(const char *, Size, bool);
        bool String(const char *str, Size len, bool);

        bool Key(const char *key, Size len, bool);
    };

    json_StreamReader st;
    Handler handler;
    rapidjson::Reader reader;

    Size depth = 0;

    bool error = false;
    bool eof = false;

public:
    json_Parser(StreamReader *st, Allocator *alloc);

    const char *GetFileName() const { return st.GetFileName(); }
    bool IsValid() const { return !error && st.IsValid(); }
    bool IsEOF() const { return eof; }

    bool ParseKey(Span<const char> *out_key);
    bool ParseKey(const char **out_key);

    bool ParseObject();
    bool InObject();
    bool ParseArray();
    bool InArray();

    bool ParseNull();
    bool ParseBool(bool *out_value);
    bool ParseInt(int64_t *out_value);
    bool ParseDouble(double *out_value);
    bool ParseString(Span<const char> *out_str);
    bool ParseString(const char **out_str);

    bool Skip();
    bool SkipNull();
    bool PassThrough(StreamWriter *writer);
    bool PassThrough(Span<char> *out_buf);
    bool PassThrough(Span<const char> *out_buf) { return PassThrough((Span<char> *)out_buf); }

    void PushLogFilter();

    json_TokenType PeekToken();
    bool ConsumeToken(json_TokenType token);

private:
    bool IncreaseDepth();
};

class json_StreamWriter {
    RG_DELETE_COPY(json_StreamWriter)

    StreamWriter *st;
    LocalArray<uint8_t, 1024> buf;

public:
    typedef char Ch;

    json_StreamWriter(StreamWriter *st) : st(st) {}

    bool IsValid() const { return st->IsValid(); }

    void Put(char c);
    void Put(Span<const char> str);
    void Flush();
};

class json_Writer: public rapidjson::Writer<json_StreamWriter> {
    RG_DELETE_COPY(json_Writer)

    json_StreamWriter writer;

public:
    json_Writer(StreamWriter *st) : rapidjson::Writer<json_StreamWriter>(writer), writer(st) {}

    bool IsValid() const { return writer.IsValid(); }

    // Hacky helpers to write long strings: call StartString() and write directly to
    // the stream. Call EndString() when done. Make sure you escape properly!
    bool StartString();
    bool EndString();

    // Same thing for raw JSON (e.g. JSON pulled from database)
    bool StartRaw();
    bool EndRaw();
    bool Raw(Span<const char> str);

    void Flush() { writer.Flush(); }
};

// This is to be used only with small static strings (e.g. enum strings)
Span<const char> json_ConvertToJsonName(Span<const char> name, Span<char> out_buf);
Span<const char> json_ConvertFromJsonName(Span<const char> name, Span<char> out_buf);

}
