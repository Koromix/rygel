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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define EXPORT __declspec(dllexport)

EXPORT int DivideBySafe(int a, int b)
{
    __try {
        return a / b;
    } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        return -42;
    }
}

EXPORT int CallThrough(int (*func)(void))
{
    int ret = func();
    return ret;
}
