// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include <stdlib.h>
#include <stdexcept>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define EXPORT extern "C" __declspec(dllexport)

EXPORT int __cdecl DoDivideBySafe1(int a, int b)
{
    __try {
        return a / b;
    } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        return -42;
    }
}

static int InnerDivide2(int a, int b)
{
    try {
        if (b == 0)
            throw std::invalid_argument("cannot divide by 0");
        return a / b;
    } catch (std::invalid_argument &) {
        return -42;
    }
}

EXPORT int __cdecl DoDivideBySafe2(int a, int b)
{
    return InnerDivide2(a, b);
}
