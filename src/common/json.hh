// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "kutil.hh"

GCC_PUSH_IGNORE(-Wconversion)
GCC_PUSH_IGNORE(-Wsign-conversion)
#include "../../lib/rapidjson/reader.h"
#include "../../lib/rapidjson/writer.h"
#include "../../lib/rapidjson/error/en.h"
GCC_POP_IGNORE()
GCC_POP_IGNORE()

class JsonStreamReader {
    StreamReader *st;

    LocalArray<char, 256 * 1024> buffer;
    Size buffer_offset = 0;
    Size file_offset = 0;

public:
    typedef char Ch MAYBE_UNUSED;

    Size line_number = 1;
    Size line_offset = 1;

    JsonStreamReader(StreamReader *st)
        : st(st)
    {
        Read();
    }

    char Peek() const { return buffer[buffer_offset]; }
    char Take()
    {
        char c = buffer[buffer_offset];
        if (c == '\n') {
            line_number++;
            line_offset = 1;
        } else {
            line_offset++;
        }
        Read();
        return c;
    }
    Size Tell() const { return file_offset + buffer_offset; }

    // Not implemented
    void Put(char) {}
    void Flush() {}
    char *PutBegin() { return 0; }
    Size PutEnd(char *) { return 0; }

    // For encoding detection only
    const char *Peek4() const
    {
        if (buffer.len - buffer_offset < 4)
            return 0;
        return buffer.data + buffer_offset;
    }

private:
    void Read()
    {
        if (buffer_offset + 1 < buffer.len) {
            buffer_offset++;
        } else if (st) {
            file_offset += buffer.len;
            buffer.len = st->Read(SIZE(buffer.data), buffer.data);
            buffer_offset = 0;

            if (buffer.len < SIZE(buffer.data)) {
                if (UNLIKELY(buffer.len < 0)) {
                    buffer.len = 0;
                }
                buffer.Append('\0');
                st = nullptr;
            }
        }
    }
};

template <typename T>
bool ParseJsonFile(StreamReader &st, T *json_handler)
{
    JsonStreamReader json_stream(&st);
    PushLogHandler([&](LogLevel level, const char *ctx,
                       const char *fmt, Span<const FmtArg> args) {
        StartConsoleLog(level);
        Print(stderr, ctx);
        Print(stderr, "%1(%2:%3): ", st.filename, json_stream.line_number, json_stream.line_offset);
        PrintFmt(fmt, args, stderr);
        PrintLn(stderr);
        EndConsoleLog();
    });
    DEFER { PopLogHandler(); };

    rapidjson::Reader json_reader;
    if (!json_reader.Parse(json_stream, *json_handler)) {
        // Parse error is likely after I/O error (missing token, etc.) but
        // it's irrelevant, the I/O error has already been issued.
        if (!st.error) {
            rapidjson::ParseErrorCode err_code = json_reader.GetParseErrorCode();
            LogError("%1 (%2)", GetParseError_En(err_code), json_reader.GetErrorOffset());
        }
        return false;
    }
    if (st.error)
        return false;

    return true;
}

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Int,
        Double,
        String
    };

    Type type;
    union {
        bool b;
        int64_t i;
        double d;
        Span<const char> str;
    } u;

    JsonValue() = default;
    JsonValue(std::nullptr_t) : type(Type::Null) {}
    JsonValue(bool b) : type(Type::Bool) { u.b = b; }
    JsonValue(int64_t i) : type(Type::Int) { u.i = i; }
    JsonValue(double d) : type(Type::Double) { u.d = d; }
    JsonValue(Span<const char> str) : type(Type::String) { u.str = str; }
};

enum class JsonBranchType {
    Array,
    EndArray,
    Object,
    EndObject
};

template <typename T>
class BaseJsonHandler {
    char current_key[60];
    bool valid_key = false;

public:
    // bool Branch(JsonBranchType, const char *) { Assert(false); }
    // bool Value(const char *, const JsonValue &)  { Assert(false); }

    bool Key(const char *key, Size, bool)
    {
        strncpy(current_key, key, SIZE(current_key));
        current_key[SIZE(current_key) - 1] = '\0';
        valid_key = true;
        return true;
    }

    bool StartArray()
    {
        DEFER { valid_key = false; };
        return ((T *)this)->Branch(JsonBranchType::Array, valid_key ? current_key : nullptr);
    }
    bool EndArray(Size)
    {
        return ((T *)this)->Branch(JsonBranchType::EndArray, valid_key ? current_key : nullptr);
    }

    bool StartObject()
    {
        DEFER { valid_key = false; };
        return ((T *)this)->Branch(JsonBranchType::Object, valid_key ? current_key : nullptr);
    }
    bool EndObject(Size)
    {
        return ((T *)this)->Branch(JsonBranchType::EndObject, valid_key ? current_key : nullptr);
    }

    bool Null()
    {
        DEFER { valid_key = false; };
        return ((T *)this)->Value(valid_key ? current_key : nullptr, nullptr);
    }
    bool Bool(bool b)
    {
        DEFER { valid_key = false; };
        return ((T *)this)->Value(valid_key ? current_key : nullptr, b);
    }
    bool Int64(int64_t i)
    {
        DEFER { valid_key = false; };
        return ((T *)this)->Value(valid_key ? current_key : nullptr, i);
    }
    bool Double(double d)
    {
        DEFER { valid_key = false; };
        return ((T *)this)->Value(valid_key ? current_key : nullptr, d);
    }
    bool String(const char *str, Size len, bool)
    {
        DEFER { valid_key = false; };
        return ((T *)this)->Value(valid_key ? current_key : nullptr, MakeSpan(str, len));
    }

    bool Int(int i) { return ((T *)this)->Int64(i); }
    bool Uint(unsigned int u) { return ((T *)this)->Int64(u); }
    bool Uint64(uint64_t u)
    {
        if (u <= INT64_MAX) {
            return ((T *)this)->Int64((int64_t)u);
        } else {
            return false;
        }
    }

    bool RawNumber(const char *, Size, bool) { Assert(false); }

    template <typename U>
    bool SetInt(const JsonValue &value, U *dest)
    {
        if (value.type != JsonValue::Type::Int)
            return UnexpectedType(value.type);

        U i = (U)value.u.i;
        if (i != value.u.i) {
            LogError("Value %1 outside of range %2 - %3", value.u.i,
                     (int)std::numeric_limits<U>::min(), (int)std::numeric_limits<U>::max());
            return false;
        }
        *dest = i;
        return true;
    }

    bool SetBool(const JsonValue &value, bool *dest)
    {
        if (value.type != JsonValue::Type::Bool)
            return UnexpectedType(value.type);

        *dest = value.u.b;
        return true;
    }
    template <typename U, typename V>
    bool SetFlag(const JsonValue &value, U *dest, V flag)
    {
        if (value.type != JsonValue::Type::Bool)
            return UnexpectedType(value.type);

        if (value.u.b) {
            *dest = (U)(*dest | flag);
        } else {
            *dest = (U)(*dest & ~flag);
        }
        return true;
    }

    bool SetString(const JsonValue &value, Allocator *alloc, const char **str)
    {
        if (value.type != JsonValue::Type::String)
            return UnexpectedType(value.type);

        *str = MakeString(alloc, value.u.str).ptr;
        return true;
    }

    bool SetDate(const JsonValue &value, int flags, Date *dest)
    {
        if (value.type != JsonValue::Type::String)
            return UnexpectedType(value.type);

        Date date = Date::FromString(value.u.str.ptr, flags);
        if (!date.value)
            return false;
        *dest = date;
        return true;
    }
    bool SetDate(const JsonValue &value, Date *dest)
        { return SetDate(value, DEFAULT_PARSE_FLAGS, dest); }

    bool UnexpectedBranch(JsonBranchType type)
    {
        switch (type) {
            case JsonBranchType::Array: { LogError("Unexpected array"); } break;
            case JsonBranchType::EndArray: { LogError("Unexpected end of array"); } break;
            case JsonBranchType::Object: { LogError("Unexpected object"); } break;
            case JsonBranchType::EndObject: { LogError("Unexpected end of object"); } break;
        }
        return false;
    }
    bool UnexpectedType(JsonValue::Type type)
    {
        switch (type) {
            case JsonValue::Type::Null: { LogError("Unexpected null value"); } break;
            case JsonValue::Type::Bool: { LogError("Unexpected boolean value"); } break;
            case JsonValue::Type::Int: { LogError("Unexpected integer value"); } break;
            case JsonValue::Type::Double: { LogError("Unexpected floating point value"); } break;
            case JsonValue::Type::String: { LogError("Unexpected string value"); } break;
        }
        return false;
    }
    bool UnknownAttribute(const char *key)
    {
        LogError("Unknown attribute '%1'", key);
        return false;
    }
    bool UnexpectedValue()
    {
        LogError("Unexpected value");
        return false;
    }
};

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
