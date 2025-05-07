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

#pragma once

#include "src/core/base/base.hh"

namespace RG {

class bk_Compiler;
class bk_VirtualMachine;
union bk_PrimitiveValue;

void bk_ImportAll(bk_Compiler *out_compiler);
void bk_ImportPrint(bk_Compiler *out_compiler);
void bk_ImportMath(bk_Compiler *out_compiler);
void bk_ImportRandom(bk_Compiler *out_compiler);

void bk_DoPrint(bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args, bool quote);

}
