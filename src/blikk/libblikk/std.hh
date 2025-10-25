// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"

namespace K {

class bk_Compiler;
class bk_VirtualMachine;
union bk_PrimitiveValue;

void bk_ImportAll(bk_Compiler *out_compiler);
void bk_ImportPrint(bk_Compiler *out_compiler);
void bk_ImportMath(bk_Compiler *out_compiler);
void bk_ImportRandom(bk_Compiler *out_compiler);

void bk_DoPrint(bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args, bool quote);

}
