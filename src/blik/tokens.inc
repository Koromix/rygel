// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef TOKEN
    #error Please define TOKEN() before including tokens.inc
#endif

TOKEN(If, 256)
TOKEN(Else, 257)
TOKEN(While, 258)

TOKEN(LeftParenthesis, '(')
TOKEN(RightParenthesis, ')')
TOKEN(LeftBrace, '{')
TOKEN(RightBrace, '}')

TOKEN(Bool, 259)
TOKEN(Integer, 261)
TOKEN(Double, 262)
TOKEN(String, 263)
TOKEN(Identifier, 264)

TOKEN(Plus, '+')
TOKEN(Minus, '-')
TOKEN(Multiply, '*')
TOKEN(Divide, '/')
TOKEN(Modulo, '%')
TOKEN(Assign, '=')

TOKEN(And, '&')
TOKEN(Or, '|')
TOKEN(Xor, '^')
TOKEN(Not, '~')
TOKEN(LeftShift, 265)
TOKEN(RightShift, 266)

TOKEN(LogicNot, '!')
TOKEN(LogicAnd, 267)
TOKEN(LogicOr, 268)

TOKEN(Equal, 269)
TOKEN(NotEqual, 270)
TOKEN(Greater, '>')
TOKEN(GreaterOrEqual, 271)
TOKEN(Less, '<')
TOKEN(LessOrEqual, 272)

#undef TOKEN
