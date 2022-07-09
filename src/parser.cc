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

bool PrototypeParser::Parse(const char *str, FunctionInfo *out_func)
{
    tokens.Clear();
    offset = 0;
    valid = true;

    Tokenize(str);

    out_func->ret.type = ParseType();
    if (out_func->ret.type->primitive == PrimitiveKind::Array) {
        MarkError("You are not allowed to directly return C arrays");
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
    if (offset < tokens.len && tokens[offset] != ")" && !Match("void")) {
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
            if (param.type->primitive == PrimitiveKind::Void ||
                    param.type->primitive == PrimitiveKind::Array) {
                MarkError("Type %1 cannot be used as a parameter (try %1*?)", param.type->name);
                return false;
            }

            if ((param.directions & 2) && param.type->primitive != PrimitiveKind::Pointer) {
                MarkError("Only pointers can be used for output parameters");
                return false;
            }

            offset += (offset < tokens.len && IsIdentifier(tokens[offset]));

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
    HeapArray<char> buf(&instance->str_alloc);

    int indirect = 0;
    bool dispose = false;

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
            MarkError("Unexpected token '%1', expected type", tokens[offset]);
        } else {
            MarkError("Unexpected end of prototype, expected type");
        }
        return instance->types_map.FindValue("void", nullptr);
    }
    while (offset < tokens.len && tokens[offset] == "*" && indirect < 4) {
        offset++;
        indirect++;
    }
    if (offset < tokens.len && tokens[offset] == "!") {
        offset++;
        dispose = true;
    }
    buf.ptr[--buf.len] = 0;

    while (buf.len) {
        const TypeInfo *type = instance->types_map.FindValue(buf.ptr, nullptr);

        if (type) {
            if (type->dispose && indirect) {
                MarkError("Cannot create pointer to disposable type '%1'", type->name);
                break;
            }
            if (type->dispose && dispose) {
                MarkError("Cannot use disposable qualifier '!' with disposable type '%1'", type->name);
                break;
            }

            if (indirect) {
                type = MakePointerType(instance, type, indirect);
                RG_ASSERT(type);
            }

            if (dispose) {
                if (type->primitive != PrimitiveKind::String &&
                        type->primitive != PrimitiveKind::String16 &&
                        indirect != 1) {
                    MarkError("Cannot use disposable qualifier '!' with type '%1'", type->name);
                    break;
                }

                TypeInfo *copy = instance->types.AppendDefault();

                memcpy((void *)copy, (const void *)type, RG_SIZE(*type));
                copy->name = "<anonymous>";
                copy->members.allocator = GetNullAllocator();
                copy->dispose = [](Napi::Env, const TypeInfo *, const void *ptr) { free((void *)ptr); };

                type = copy;
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

bool ParsePrototype(Napi::Env env, const char *str, FunctionInfo *out_func)
{
    PrototypeParser parser(env);
    return parser.Parse(str, out_func);
}

}
