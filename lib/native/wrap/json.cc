// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "json.hh"
#include "vendor/fast_float/fast_float.h"

namespace K {

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

bool json_Parser::Handler::StartObject()
{
    token = json_TokenType::StartObject;
    return true;
}

bool json_Parser::Handler::EndObject(Size)
{
    token = json_TokenType::EndObject;
    return true;
}

bool json_Parser::Handler::StartArray()
{
    token = json_TokenType::StartArray;
    return true;
}

bool json_Parser::Handler::EndArray(Size)
{
    token = json_TokenType::EndArray;
    return true;
}

bool json_Parser::Handler::Null()
{
    token = json_TokenType::Null;
    return true;
}

bool json_Parser::Handler::Bool(bool b)
{
    token = json_TokenType::Bool;
    u.b = b;
    return true;
}

bool json_Parser::Handler::RawNumber(const char *str, Size len, bool)
{
    token = json_TokenType::Number;

    u.num.len = std::min(len, K_SIZE(u.num.data) - 1);
    MemCpy(u.num.data, str, u.num.len);
    u.num.data[u.num.len] = 0;

    return true;
}

bool json_Parser::Handler::String(const char *str, Size len, bool)
{
    token = json_TokenType::String;
    u.str = DuplicateString(MakeSpan(str, len), allocator);
    return true;
}

bool json_Parser::Handler::Key(const char *key, Size len, bool)
{
    token = json_TokenType::Key;
    u.str = DuplicateString(MakeSpan(key, len), allocator);
    return true;
}

json_Parser::json_Parser(StreamReader *st, Allocator *alloc)
    : st(st), handler({ alloc })
{
    K_ASSERT(alloc);
    reader.IterativeParseInit();
}

bool json_Parser::ParseKey(Span<const char> *out_key)
{
    if (ConsumeToken(json_TokenType::Key)) {
        *out_key = handler.u.str;
        return true;
    } else {
        return false;
    }
}

bool json_Parser::ParseKey(const char **out_key)
{
    if (ConsumeToken(json_TokenType::Key)) {
        *out_key = handler.u.str.ptr;
        return true;
    } else {
        return false;
    }
}

Span<const char> json_Parser::ParseKey()
{
    if (ConsumeToken(json_TokenType::Key)) {
        return handler.u.str;
    } else {
        return {};
    }
}

bool json_Parser::ParseObject()
{
    return ConsumeToken(json_TokenType::StartObject) &&
           IncreaseDepth();
}

bool json_Parser::InObject()
{
    if (PeekToken() == json_TokenType::EndObject) {
        depth--;
        handler.token = json_TokenType::Invalid;
    }

    return handler.token != json_TokenType::Invalid;
}

bool json_Parser::ParseArray()
{
    return ConsumeToken(json_TokenType::StartArray) &&
           IncreaseDepth();
}

bool json_Parser::InArray()
{
    if (PeekToken() == json_TokenType::EndArray) {
        depth--;
        handler.token = json_TokenType::Invalid;
    }

    return handler.token != json_TokenType::Invalid;
}

bool json_Parser::ParseNull()
{
    return ConsumeToken(json_TokenType::Null);
}

bool json_Parser::ParseBool(bool *out_b)
{
    if (ConsumeToken(json_TokenType::Bool)) {
        *out_b = handler.u.b;
        return true;
    } else {
        return false;
    }
}

bool json_Parser::ParseInt(int64_t *out_i)
{
    if (ConsumeToken(json_TokenType::Number)) {
        error |= !K::ParseInt(handler.u.num, out_i);
        return !error;
    } else {
        return false;
    }
}

bool json_Parser::ParseInt(int *out_i)
{
    if (ConsumeToken(json_TokenType::Number)) {
        error |= !K::ParseInt(handler.u.num, out_i);
        return !error;
    } else {
        return false;
    }
}

bool json_Parser::ParseDouble(double *out_d)
{
    if (ConsumeToken(json_TokenType::Number)) {
        fast_float::from_chars_result ret = fast_float::from_chars(handler.u.num.data, handler.u.num.end(), *out_d);

        if (ret.ec != std::errc()) [[unlikely]] {
            LogError("Malformed float number");
            error = true;
        }

        return !error;
    } else {
        return false;
    }
}

bool json_Parser::ParseString(Span<const char> *out_str)
{
    if (ConsumeToken(json_TokenType::String)) {
        *out_str = handler.u.str;
        return true;
    } else {
        return false;
    }
}

bool json_Parser::ParseString(const char **out_str)
{
    if (ConsumeToken(json_TokenType::String)) {
        *out_str = handler.u.str.ptr;
        return true;
    } else {
        return false;
    }
}

Span<const char> json_Parser::ParseString()
{
    if (ConsumeToken(json_TokenType::String)) {
        return handler.u.str;
    } else {
        return {};
    }
}

bool json_Parser::IsNumberFloat() const
{
    if (handler.token != json_TokenType::Number)
        return false;

    return strchr(handler.u.num.data, '.');
}

bool json_Parser::Skip()
{
    switch (PeekToken()) {
        case json_TokenType::Invalid: return false;

        case json_TokenType::StartObject: {
            for (ParseObject(); InObject(); ) {
                Skip();
            }
        } break;
        case json_TokenType::EndObject: { K_ASSERT(error); } break;
        case json_TokenType::StartArray: {
            for (ParseArray(); InArray(); ) {
                Skip();
            }
        } break;
        case json_TokenType::EndArray: { K_ASSERT(error); } break;

        case json_TokenType::Null:
        case json_TokenType::Bool:
        case json_TokenType::Number:
        case json_TokenType::String: { handler.token = json_TokenType::Invalid; } break;

        case json_TokenType::Key: {
            handler.token = json_TokenType::Invalid;
            Skip();
        } break;
    }

    return IsValid();
}

bool json_Parser::SkipNull()
{
    if (PeekToken() == json_TokenType::Null) {
        handler.token = json_TokenType::Invalid;
        return true;
    } else {
        return false;
    }
}

class CopyHandler {
    json_Writer json;
    int depth = 0;

public:
    CopyHandler(StreamWriter *writer) : json(writer) {}

    int GetDepth() const { return depth; }

    bool StartObject() { json.StartObject(); depth++; return json.IsValid(); }
    bool EndObject(Size) { json.EndObject(); depth--; return json.IsValid(); }
    bool StartArray() { json.StartArray(); depth++; return json.IsValid(); }
    bool EndArray(Size) { json.EndArray(); depth--; return json.IsValid(); }

    bool Null() { json.Null(); return json.IsValid(); }
    bool Bool(bool b) { json.Bool(b); return json.IsValid(); }
    bool RawNumber(const char *str, Size len, bool) { json.RawNumber(str, len); return json.IsValid(); }
    bool String(const char *str, Size len, bool) { json.String(str, len); return json.IsValid(); }

    bool Key(const char *key, Size len, bool) { json.Key(key, len); return json.IsValid(); }

    bool Double(double) { K_UNREACHABLE(); }
    bool Int(int) { K_UNREACHABLE(); }
    bool Int64(int64_t) { K_UNREACHABLE(); }
    bool Uint(unsigned int) { K_UNREACHABLE(); }
    bool Uint64(uint64_t) { K_UNREACHABLE(); }
};

bool json_Parser::PassThrough(StreamWriter *writer)
{
    if (error) [[unlikely]]
        return false;

    CopyHandler copier(writer);
    bool empty = true;

    if (handler.token == json_TokenType::Invalid) {
        const unsigned int flags = rapidjson::kParseNumbersAsStringsFlag | rapidjson::kParseStopWhenDoneFlag;
        empty &= !reader.IterativeParseNext<flags>(st, copier);
    } else {
        switch (handler.token) {
            case json_TokenType::Invalid: { K_UNREACHABLE(); } break;

            case json_TokenType::StartObject: { copier.StartObject(); } break;
            case json_TokenType::EndObject: { copier.EndObject(0); } break;
            case json_TokenType::StartArray: { copier.StartArray(); } break;
            case json_TokenType::EndArray: { copier.EndArray(0); } break;

            case json_TokenType::Null: { copier.Null(); } break;
            case json_TokenType::Bool: { copier.Bool(handler.u.b); } break;
            case json_TokenType::Number: { copier.RawNumber(handler.u.num.data, handler.u.num.len, true); } break;
            case json_TokenType::String: { copier.String(handler.u.str.ptr, handler.u.str.len, true); } break;

            case json_TokenType::Key: { copier.Key(handler.u.str.ptr, handler.u.str.len, true); } break;
        }

        handler.token = json_TokenType::Invalid;
        empty = false;
    }

    const unsigned int flags = rapidjson::kParseNumbersAsStringsFlag | rapidjson::kParseStopWhenDoneFlag;
    while (copier.GetDepth() && reader.IterativeParseNext<flags>(st, copier));

    if (reader.HasParseError()) {
        rapidjson::ParseErrorCode err = reader.GetParseErrorCode();
        LogError(("%1"), GetParseError_En(err));

        error = true;
    } else if (reader.IterativeParseComplete()) {
        eof = true;

        if (empty || copier.GetDepth()) {
            LogError("Unexpected end of JSON file");
            error = true;
        }
    }

    return !error;
}

bool json_Parser::PassThrough(Span<char> *out_buf)
{
    HeapArray<uint8_t> buf(handler.allocator);
    StreamWriter st(&buf, "<through>");

    if (!PassThrough(&st))
        return false;

    buf.Grow(1);
    buf.ptr[buf.len] = 0;

    *out_buf = buf.Leak().As<char>();
    return true;
}

bool json_Parser::PassThrough(const char **out_str)
{
    Span<char> buf;
    if (!PassThrough(&buf))
        return false;

    *out_str = buf.ptr;
    return true;
}

void json_Parser::UnexpectedKey(Span<const char> key)
{
    if (!IsValid())
        return;

    LogError("Unexpected key '%1'", key);
    Skip();
}

void json_Parser::PushLogFilter()
{
    K::PushLogFilter([this](LogLevel level, const char *, const char *msg, FunctionRef<LogFunc> func) {
        char ctx[1024];
        Fmt(ctx, "%1(%2:%3): ", st.GetFileName(), st.GetLineNumber(), st.GetLineOffset());

        func(level, ctx, msg);
    });
}

json_TokenType json_Parser::PeekToken()
{
    if (error) [[unlikely]]
        return json_TokenType::Invalid;

    if (handler.token == json_TokenType::Invalid) {
        const unsigned int flags = rapidjson::kParseNumbersAsStringsFlag | rapidjson::kParseStopWhenDoneFlag;
        if (!reader.IterativeParseNext<flags>(st, handler)) {
            if (reader.HasParseError()) {
                if (!error) {
                    rapidjson::ParseErrorCode err = reader.GetParseErrorCode();
                    LogError(("%1"), GetParseError_En(err));
                }
                error = true;
            } else {
                eof = true;
            }
        }
    }

    return handler.token;
}

bool json_Parser::ConsumeToken(json_TokenType token)
{
    if (PeekToken() != token && !error) {
        LogError("Unexpected token '%1', expected '%2'",
                 json_TokenTypeNames[(int)handler.token], json_TokenTypeNames[(int)token]);
        error = true;
    }

    handler.token = json_TokenType::Invalid;
    return !error;
}

bool json_Parser::IncreaseDepth()
{
    if (depth >= 16) [[unlikely]] {
        LogError("Excessive depth for JSON object or array");
        error = true;
        return false;
    }

    depth++;
    return true;
}

Span<const char> json_ConvertToJsonName(Span<const char> name, Span<char> out_buf)
{
    K_ASSERT(out_buf.len >= 2);

    if (name.len) {
        out_buf[0] = LowerAscii(name[0]);

        Size j = 1;
        for (Size i = 1; i < name.len && j < out_buf.len - 2; i++) {
            char c = name[i];

            if (c >= 'A' && c <= 'Z') {
                out_buf[j++] = '_';
                out_buf[j++] = LowerAscii(c);
            } else {
                out_buf[j++] = c;
            }
        }
        out_buf[j] = 0;

        return MakeSpan(out_buf.ptr, j);
    } else {
        out_buf[0] = 0;
        return MakeSpan(out_buf.ptr, 0);
    }
}

Span<const char> json_ConvertFromJsonName(Span<const char> name, Span<char> out_buf)
{
    K_ASSERT(out_buf.len >= 2);

    if (name.len) {
        out_buf[0] = UpperAscii(name[0]);

        Size j = 1;
        for (Size i = 1; i < name.len && j < out_buf.len - 1; i++) {
            char c = name[i];

            if (c == '_' && i + 1 < name.len) {
                out_buf[j++] = UpperAscii(name[++i]);
            } else {
                out_buf[j++] = c;
            }
        }
        out_buf[j] = 0;

        return MakeSpan(out_buf.ptr, j);
    } else {
        out_buf[0] = 0;
        return MakeSpan(out_buf.ptr, 0);
    }
}

}
