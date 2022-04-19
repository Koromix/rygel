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

#include <stdio.h>

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif
#if defined(_M_IX86) || defined(__i386__)
    #ifdef _MSC_VER
        #define FASTCALL __fastcall
    #else
        #define FASTCALL __attribute__((fastcall))
    #endif
#else
    #define FASTCALL
#endif

typedef struct Pack3 {
    int a;
    int b;
    int c;
} Pack3;

EXPORT void FillPack3(int a, int b, int c, Pack3 *p)
{
    p->a = a;
    p->b = b;
    p->c = c;
}

EXPORT void FASTCALL AddPack3(int a, int b, int c, Pack3 *p)
{
    p->a += a;
    p->b += b;
    p->c += c;
}
