// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef TOKEN
    #error Please define TOKEN() before including tokens.inc
#endif

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
TOKEN(Do, "do")
TOKEN(LeftParenthesis, "(")
TOKEN(RightParenthesis, ")")
TOKEN(Comma, ",")

TOKEN(Null, "null")
TOKEN(Bool, "<bool>")
TOKEN(Integer, "<integer>")
TOKEN(Double, "<double>")
TOKEN(String, "<string>")
TOKEN(Identifier, "<identifier>")

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

TOKEN(Dot, ".")
TOKEN(DotDot, "..")
TOKEN(DotDotDot, "...")
TOKEN(Colon, ":")
TOKEN(Reassign, ":=")
TOKEN(Equal, "=")
TOKEN(NotEqual, "!=")
TOKEN(Greater, ">")
TOKEN(GreaterOrEqual, ">=")
TOKEN(Less, "<")
TOKEN(LessOrEqual, "<=")

TOKEN(NewLine, "<newline>")

#undef TOKEN
