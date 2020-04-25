// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef TOKEN
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

// Immediate values
TOKEN(Null, "null")
TOKEN(Bool, "<bool>")
TOKEN(Integer, "<integer>")
TOKEN(Float, "<float>")
TOKEN(String, "<string>")
TOKEN(Identifier, "<identifier>")

// Operators
TOKEN(Dot, ".")
TOKEN(DotDot, "..")
TOKEN(DotDotDot, "...")
TOKEN(Colon, ":")
TOKEN(LeftParenthesis, "(")
TOKEN(RightParenthesis, ")")
TOKEN(Plus, "+")
TOKEN(Minus, "-")
TOKEN(Multiply, "*")
TOKEN(Divide, "/")
TOKEN(Modulo, "%")
TOKEN(And, "&")
TOKEN(Or, "|")
TOKEN(Xor, "^")
TOKEN(Not, "~")
TOKEN(LeftShift, "<<")
TOKEN(RightShift, ">>")
TOKEN(LogicNot, "!")
TOKEN(LogicAnd, "&&")
TOKEN(LogicOr, "||")
TOKEN(Reassign, ":=")
TOKEN(Equal, "=")
TOKEN(NotEqual, "!=")
TOKEN(Greater, ">")
TOKEN(GreaterOrEqual, ">=")
TOKEN(Less, "<")
TOKEN(LessOrEqual, "<=")
TOKEN(Comma, ",")

// End statements
TOKEN(EndOfLine, "<end of line>")

#undef TOKEN
