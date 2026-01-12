// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
K_PUSH_NO_WARNINGS
#define RAPIDJSON_NO_SIZETYPEDEFINE
namespace rapidjson { typedef K::Size SizeType; }
#include "vendor/rapidjson/reader.h"
#include "vendor/rapidjson/writer.h"
#include "vendor/rapidjson/prettywriter.h"
#include "vendor/rapidjson/error/en.h"
K_POP_NO_WARNINGS

namespace K {

class json_StreamReader {
    K_DELETE_COPY(json_StreamReader)

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
    char *PutBegin() { return nullptr; }
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
    K_DELETE_COPY(json_Parser)

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
        bool Double(double) { K_UNREACHABLE(); }
        bool Int(int) { K_UNREACHABLE(); }
        bool Int64(int64_t) { K_UNREACHABLE(); }
        bool Uint(unsigned int) { K_UNREACHABLE(); }
        bool Uint64(uint64_t) { K_UNREACHABLE(); }
        bool RawNumber(const char *, Size, bool);
        bool String(const char *str, Size len, bool);

        bool Key(const char *key, Size len, bool);
    };

    json_StreamReader st;
    Handler handler;
    rapidjson::Reader reader;

    int depth = 0;

    bool error = false;
    bool eof = false;

public:
    json_Parser(StreamReader *st, Allocator *alloc);

    const char *GetFileName() const { return st.GetFileName(); }
    bool IsValid() const { return !error && st.IsValid(); }
    bool IsEOF() const { return eof; }

    bool ParseKey(Span<const char> *out_key);
    bool ParseKey(const char **out_key);
    Span<const char> ParseKey();

    bool ParseObject();
    bool InObject();
    bool ParseArray();
    bool InArray();

    bool ParseNull();
    bool ParseBool(bool *out_value);
    bool ParseInt(int64_t *out_value);
    bool ParseInt(int *out_value);
    bool ParseDouble(double *out_value);
    bool ParseString(Span<const char> *out_str);
    bool ParseString(const char **out_str);
    Span<const char> ParseString();

    bool IsNumberFloat() const;

    bool Skip();
    bool SkipNull();
    bool PassThrough(StreamWriter *writer);
    bool PassThrough(Span<char> *out_buf);
    bool PassThrough(Span<const char> *out_buf) { return PassThrough((Span<char> *)out_buf); }
    bool PassThrough(const char **out_str);
    Span<const char> PassThrough();

    void UnexpectedKey(Span<const char> key);

    void PushLogFilter();

    json_TokenType PeekToken();
    bool ConsumeToken(json_TokenType token);

private:
    bool IncreaseDepth();
};

class json_StreamWriter {
    K_DELETE_COPY(json_StreamWriter)

    StreamWriter *st;
    LocalArray<uint8_t, 1024> buf;

public:
    typedef char Ch;

    json_StreamWriter(StreamWriter *st) : st(st) {}

    bool IsValid() const { return st->IsValid(); }

    void Put(char c)
    {
        buf.Append((uint8_t)c);

        if (buf.len == K_SIZE(buf.data)) {
            st->Write(buf);
            buf.Clear();
        }
    }

    void Put(Span<const char> str)
    {
        Flush();
        st->Write(str);
    }

    void Flush()
    {
        st->Write(buf);
        buf.Clear();
    }
};

template <typename T = rapidjson::Writer<json_StreamWriter>>
class json_WriterBase: public T {
    K_DELETE_COPY(json_WriterBase)

    json_StreamWriter writer;

public:
    json_WriterBase(StreamWriter *st) : T(writer), writer(st) {}

    bool IsValid() const { return writer.IsValid(); }

    bool StringOrNull(const char *str)
    {
        if (str) {
            T::String(str);
        } else {
            T::Null();
        }

        return true;
    }

    // Hacky helpers to write long strings: call StartString() and write directly to
    // the stream. Call EndString() when done. Make sure you escape properly!
    bool StartString()
    {
        T::Prefix(rapidjson::kStringType);
        writer.Put('"');
        writer.Flush();

        return true;
    }

    bool EndString()
    {
        writer.Put('"');
        return true;
    }

    // Same thing for raw JSON (e.g. JSON pulled from database)
    bool StartRaw()
    {
        T::Prefix(rapidjson::kStringType);
        writer.Flush();

        return true;
    }

    bool EndRaw()
    {
        return true;
    }

    bool Raw(Span<const char> str)
    {
        StartRaw();
        writer.Put(str);
        EndRaw();

        return true;
    }

    void Flush() { writer.Flush(); }
};

class json_CompactWriter: public json_WriterBase<rapidjson::Writer<json_StreamWriter>> {
public:
    json_CompactWriter(StreamWriter *st) : json_WriterBase(st) {}
};

class json_PrettyWriter: public json_WriterBase<rapidjson::PrettyWriter<json_StreamWriter>> {
public:
    json_PrettyWriter(StreamWriter *st, int indent = 2)
        : json_WriterBase(st)
    {
        SetIndent(' ', indent);
    }
};

#if defined(K_DEBUG)
typedef json_PrettyWriter json_Writer;
#else
typedef json_CompactWriter json_Writer;
#endif

// This is to be used only with small static strings (e.g. enum strings)
Span<const char> json_ConvertToJsonName(Span<const char> name, Span<char> out_buf);
Span<const char> json_ConvertFromJsonName(Span<const char> name, Span<char> out_buf);

}
