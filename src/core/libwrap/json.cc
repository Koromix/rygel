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
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"
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

bool json_Parser::Handler::Double(double d)
{
    token = json_TokenType::Double;
    u.d = d;
    return true;
}

bool json_Parser::Handler::Int(int i)
{
    token = json_TokenType::Integer;
    u.i = (int64_t)i;
    return true;
}

bool json_Parser::Handler::Int64(int64_t i)
{
    token = json_TokenType::Integer;
    u.i = i;
    return true;
}

bool json_Parser::Handler::Uint(unsigned int i)
{
    token = json_TokenType::Integer;
    u.i = (int64_t)i;
    return true;
}

bool json_Parser::Handler::Uint64(uint64_t i)
{
    if (RG_UNLIKELY(i > INT64_MAX)) {
        LogError("Integer value %1 is too big", i);
        return false;
    }

    token = json_TokenType::Integer;
    u.i = (int64_t)i;
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
    u.key = DuplicateString(MakeSpan(key, len), allocator);
    return true;
}

json_Parser::json_Parser(StreamReader *st, Allocator *alloc)
    : st(st), handler({alloc})
{
    RG_ASSERT(alloc);
    reader.IterativeParseInit();
}

bool json_Parser::ParseKey(Span<const char> *out_key)
{
    if (ConsumeToken(json_TokenType::Key)) {
        *out_key = handler.u.key;
        return true;
    } else {
        return false;
    }
}

bool json_Parser::ParseKey(const char **out_key)
{
    if (ConsumeToken(json_TokenType::Key)) {
        *out_key = handler.u.key.ptr;
        return true;
    } else {
        return false;
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
    if (ConsumeToken(json_TokenType::Integer)) {
        *out_i = handler.u.i;
        return true;
    } else {
        return false;
    }
}

bool json_Parser::ParseDouble(double *out_d)
{
    if (ConsumeToken(json_TokenType::Double)) {
        *out_d = handler.u.d;
        return true;
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

void json_Parser::PushLogFilter()
{
    RG::PushLogFilter([this](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        char ctx_buf[1024];
        Fmt(ctx_buf, "%1(%2:%3)%4%5", st.GetFileName(), st.GetLineNumber(), st.GetLineOffset(),
                                      ctx ? ": " : "", ctx ? ctx : "");

        func(level, ctx_buf, msg);
    });
}

json_TokenType json_Parser::PeekToken()
{
    if (handler.token == json_TokenType::Invalid &&
            !reader.IterativeParseNext<rapidjson::kParseDefaultFlags>(st, handler)) {
        if (reader.HasParseError()) {
            if (!error) {
                rapidjson::ParseErrorCode err = reader.GetParseErrorCode();
                LogError("%1", GetParseError_En(err));
            }
            error = true;
        } else {
            eof = true;
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
    if (RG_UNLIKELY(depth >= 8)) {
        LogError("Excessive depth for JSON object or array");
        error = true;
        return false;
    }

    depth++;
    return true;
}

void json_StreamWriter::Put(char c)
{
    buf.Append((uint8_t)c);

    if (buf.len == RG_SIZE(buf.data)) {
        st->Write(buf);
        buf.Clear();
    }
}

void json_StreamWriter::Put(Span<const char> str)
{
    Flush();
    st->Write(str);
}

void json_StreamWriter::Flush()
{
    st->Write(buf);
    buf.Clear();
}

bool json_Writer::StartString()
{
    Prefix(rapidjson::kStringType);
    writer.Put('"');
    writer.Flush();

    return true;
}

bool json_Writer::EndString()
{
    writer.Put('"');
    return true;
}

bool json_Writer::StartRaw()
{
    Prefix(rapidjson::kStringType);
    writer.Flush();

    return true;
}

bool json_Writer::EndRaw()
{
    return true;
}

bool json_Writer::Raw(Span<const char> str)
{
    StartRaw();
    writer.Put(str);
    EndRaw();

    return true;
}

Span<const char> ConvertToJsonName(Span<const char> name, Span<char> out_buf)
{
    RG_ASSERT(out_buf.len >= 2);

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

Span<const char> ConvertFromJsonName(Span<const char> name, Span<char> out_buf)
{
    RG_ASSERT(out_buf.len >= 2);

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
