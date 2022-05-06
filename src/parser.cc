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
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "vendor/libcc/libcc.hh"
#include "ffi.hh"
#include "parser.hh"

#include <napi.h>

namespace RG {

bool PrototypeParser::Parse(Napi::String proto, FunctionInfo *func)
{
    if (!proto.IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for prototype, expected string", GetValueType(instance, proto));
        return false;
    }

    tokens.Clear();
    offset = 0;
    valid = true;

    Tokenize(std::string(proto).c_str());

    func->ret.type = ParseType();
    if (Match("__cdecl")) {
        func->convention = CallConvention::Cdecl;
    } else if (Match("__stdcall")) {
        func->convention = CallConvention::Stdcall;
    } else if (Match("__fastcall")) {
        func->convention = CallConvention::Fastcall;
    }
    func->name = ParseIdentifier();

    Consume("(");
    if (offset < tokens.len && tokens[offset] != ")") {
        for (;;) {
            ParameterInfo param = {};

            if (Match("...")) {
                func->variadic = true;
                break;
            }

            if (Match("_In_")) {
                param.directions = 1;
            } else if (Match("_Out_")) {
                param.directions = 2;
            } else if (Match("_Inout_")) {
                param.directions = 3;
            } else {
                param.directions = 1;
            }

            param.type = ParseType();
            if (param.type->primitive == PrimitiveKind::Void) {
                MarkError("Type void cannot be used as a parameter");
                return false;
            }

            if ((param.directions & 2) && (param.type->primitive != PrimitiveKind::Pointer ||
                                           param.type->ref->primitive != PrimitiveKind::Record)) {
                MarkError("Only object pointers can be used as out parameters (for now)");
                return false;
            }

            offset += (offset < tokens.len && IsIdentifier(tokens[offset]));

            if (func->parameters.len >= MaxParameters) {
                MarkError("Functions cannot have more than %1 parameters", MaxParameters);
                return false;
            }
            if ((param.directions & 2) && ++func->out_parameters >= MaxOutParameters) {
                MarkError("Functions cannot have more than out %1 parameters", MaxOutParameters);
                return false;
            }

            param.offset = func->parameters.len;

            func->parameters.Append(param);

            if (offset >= tokens.len || tokens[offset] != ",")
                break;
            offset++;
        }
    }
    Consume(")");

    Match(";");
    if (offset < tokens.len) {
        MarkError("Unexpected token '%1' after prototype", tokens[offset]);
    }

    return valid;
}

void PrototypeParser::Tokenize(const char *str)
{
    for (Size i = 0; str[i]; i++) {
        char c = str[i];

        if (IsAsciiWhite(c)) {
            continue;
        } else if (IsAsciiAlpha(c) || c == '_') {
            Size j = i;
            while (str[++j] && (IsAsciiAlphaOrDigit(str[j]) || str[j] == '_'));

            Span<const char> tok = MakeSpan(str + i, j - i);
            tokens.Append(tok);

            i = j - 1;
        } else if (c == '.' && str[i + 1] == '.' && str[i + 2] == '.') {
            tokens.Append("...");
            i += 2;
        } else {
            Span<const char> tok = MakeSpan(str + i, 1);
            tokens.Append(tok);
        }
    }
}

const TypeInfo *PrototypeParser::ParseType()
{
    HeapArray<char> buf(&instance->str_alloc);

    Size indirect = 0;

    Size start = offset;
    while (offset < tokens.len && IsIdentifier(tokens[offset])) {
        Span<const char> tok = tokens[offset++];

        if (tok != "const") {
            buf.Append(tok);
            buf.Append(' ');
        }
    }
    if (offset == start) {
        if (offset < tokens.len) {
            MarkError("Unexpected token '%1', expected identifier", tokens[offset]);
        } else {
            MarkError("Unexpected end of prototype, expected identifier");
        }
        return instance->types_map.FindValue("void", nullptr);
    }
    while (offset < tokens.len && tokens[offset] == "*") {
        offset++;
        indirect++;
    }
    buf.ptr[--buf.len] = 0;

    if (TestStr(buf, "char") && indirect) {
        buf.RemoveFrom(0);
        Fmt(&buf, "string");

        indirect--;
    }

    while (buf.len) {
        const TypeInfo *type = instance->types_map.FindValue(buf.ptr, nullptr);

        if (type) {
            for (Size i = 0; i < indirect; i++) {
                type = GetPointerType(instance, type);
                RG_ASSERT(type);
            }
            return type;
        }

        // Truncate last token
        {
            Span<const char> remain;
            SplitStrReverse(buf, ' ', &remain);
            buf.len = remain.len;
            buf.ptr[buf.len] = 0;
        }

        if (indirect) {
            offset -= indirect;
            indirect = 0;
        }
        offset--;
    }

    MarkError("Unknown type '%1'", tokens[start]);
    return instance->types_map.FindValue("void", nullptr);
}

const char *PrototypeParser::ParseIdentifier()
{
    if (offset >= tokens.len) {
        MarkError("Unexpected end of prototype, expected identifier");
        return "";
    }
    if (!IsIdentifier(tokens[offset])) {
        MarkError("Unexpected token '%1', expected identifier", tokens[offset]);
        return "";
    }

    Span<const char> tok = tokens[offset++];
    const char *ident = DuplicateString(tok, &instance->str_alloc).ptr;

    return ident;
}

bool PrototypeParser::Consume(const char *expect)
{
    if (offset >= tokens.len) {
        MarkError("Unexpected end of prototype, expected '%1'", expect);
        return false;
    }
    if (tokens[offset] != expect) {
        MarkError("Unexpected token '%1', expected '%2'", tokens[offset], expect);
        return false;
    }

    offset++;
    return true;
}

bool PrototypeParser::Match(const char *expect)
{
    if (offset < tokens.len && tokens[offset] == expect) {
        offset++;
        return true;
    } else {
        return false;
    }
}

bool PrototypeParser::IsIdentifier(Span<const char> tok) const
{
    RG_ASSERT(tok.len);
    return IsAsciiAlpha(tok[0]) || tok[0] == '_';
}

}
