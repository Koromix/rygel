// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#if !defined(TOKEN)
    #error Please define TOKEN() before including tokens.inc
#endif

// Keywords
TOKEN(Func, "func")
TOKEN(Return, "return")
TOKEN(Let, "let")
TOKEN(Mut, "mut")
TOKEN(Begin, "begin")
TOKEN(End, "end")
TOKEN(If, "if")
TOKEN(Else, "else")
TOKEN(While, "while")
TOKEN(For, "for")
TOKEN(In, "in")
TOKEN(Break, "break")
TOKEN(Continue, "continue")
TOKEN(Do, "do")
TOKEN(Record, "record")
TOKEN(Enum, "enum")
TOKEN(Pass, "pass")

// Immediate values
TOKEN(Null, "null")
TOKEN(Boolean, "<bool>")
TOKEN(Integer, "<integer>")
TOKEN(Float, "<float>")
TOKEN(String, "<string>")
TOKEN(Identifier, "<identifier>")

// Operators
TOKEN(Dot, ".")
TOKEN(Colon, ":")
TOKEN(LeftParenthesis, "(")
TOKEN(RightParenthesis, ")")
TOKEN(LeftBracket, "[")
TOKEN(RightBracket, "]")
TOKEN(Plus, "+")
TOKEN(Minus, "-")
TOKEN(Multiply, "*")
TOKEN(Divide, "/")
TOKEN(Modulo, "%")
TOKEN(And, "&")
TOKEN(Or, "|")
TOKEN(XorOrComplement, "~")
TOKEN(LeftShift, "<<")
TOKEN(RightShift, ">>")
TOKEN(LeftRotate, "<<<")
TOKEN(RightRotate, ">>>")
TOKEN(AndAnd, "and")
TOKEN(OrOr, "or")
TOKEN(Not, "not")
TOKEN(Equal, "=")
TOKEN(Reassign, ":=")
TOKEN(PlusAssign, "+=")
TOKEN(MinusAssign, "-=")
TOKEN(MultiplyAssign, "*=")
TOKEN(DivideAssign, "/=")
TOKEN(ModuloAssign, "%=")
TOKEN(LeftShiftAssign, "<<=")
TOKEN(RightShiftAssign, ">>=")
TOKEN(LeftRotateAssign, "<<<=")
TOKEN(RightRotateAssign, ">>>=")
TOKEN(AndAssign, "&=")
TOKEN(OrAssign, "|=")
TOKEN(XorAssign, "~=")
TOKEN(NotEqual, "!=")
TOKEN(Greater, ">")
TOKEN(GreaterOrEqual, ">=")
TOKEN(Less, "<")
TOKEN(LessOrEqual, "<=")
TOKEN(Comma, ",")

// End statements
TOKEN(EndOfLine, "<end of line>")
TOKEN(Semicolon, ";")

#undef TOKEN
