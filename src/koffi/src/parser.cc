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

#include "src/core/libcc/libcc.hh"
#include "ffi.hh"
#include "parser.hh"

#include <napi.h>

namespace RG {

bool PrototypeParser::Parse(const char *str, FunctionInfo *out_func)
{
    tokens.Clear();
    offset = 0;
    valid = true;

    Tokenize(str);

    out_func->ret.type = ParseType();
    if (!CanReturnType(out_func->ret.type)) {
        MarkError("You are not allowed to directly return %1 values (maybe try %1 *)", out_func->ret.type->name);
        return false;
    }
    if (Match("__cdecl")) {
        out_func->convention = CallConvention::Cdecl;
    } else if (Match("__stdcall")) {
        out_func->convention = CallConvention::Stdcall;
    } else if (Match("__fastcall")) {
        out_func->convention = CallConvention::Fastcall;
    } else if (Match("__thiscall")) {
        out_func->convention = CallConvention::Thiscall;
    }
    out_func->name = ParseIdentifier();

    Consume("(");
    offset += (offset + 1 < tokens.len && tokens[offset] == "void" && tokens[offset + 1] == ")");
    if (offset < tokens.len && tokens[offset] != ")") {
        for (;;) {
            ParameterInfo param = {};

            if (Match("...")) {
                out_func->variadic = true;
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

            if (!CanPassType(param.type, param.directions)) {
                MarkError("Type %1 cannot be used as a parameter (maybe try %1 *)", param.type->name);
                return false;
            }
            if (out_func->parameters.len >= MaxParameters) {
                MarkError("Functions cannot have more than %1 parameters", MaxParameters);
                return false;
            }
            if ((param.directions & 2) && ++out_func->out_parameters >= MaxOutParameters) {
                MarkError("Functions cannot have more than out %1 parameters", MaxOutParameters);
                return false;
            }

            param.offset = (int8_t)out_func->parameters.len;

            out_func->parameters.Append(param);

            offset += (offset < tokens.len && IsIdentifier(tokens[offset]));
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
        } else if (IsAsciiDigit(c)) {
            Size j = i;
            while (str[++j] && IsAsciiDigit(str[j]));
            if (str[j] == '.') {
                while (str[++j] && IsAsciiDigit(str[j]));
            }

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
    Size start = offset;

    if (offset >= tokens.len) {
        MarkError("Unexpected end of prototype, expected type");
        return instance->types_map.FindValue("void", nullptr);
    } else if (!IsIdentifier(tokens[offset])) {
        MarkError("Unexpected token '%1', expected type", tokens[offset]);
        return instance->types_map.FindValue("void", nullptr);
    }

    while (offset < tokens.len && (IsIdentifier(tokens[offset]) ||
                                   tokens[offset] == '*')) {
        offset++;
    }
    offset += (offset < tokens.len && tokens[offset] == "!");

    while (offset >= start) {
        Span<const char> str = MakeSpan(tokens[start].ptr, tokens[offset].end() - tokens[start].ptr);
        const TypeInfo *type = ResolveType(instance, str);

        if (type) {
            offset++;
            return type;
        }

        offset--;
    }
    offset = start;

    MarkError("Unknown or invalid type name '%1'", tokens[offset]);
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

bool ParsePrototype(Napi::Env env, const char *str, FunctionInfo *out_func)
{
    PrototypeParser parser(env);
    return parser.Parse(str, out_func);
}

}
