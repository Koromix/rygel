// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "../libblikk/libblikk.hh"

namespace K {

struct Config {
    bool try_expression = true;
    bool execute = true;
    bool debug = false;
};

int RunFile(const char *filename, const Config &config);
int RunCommand(Span<const char> code, const Config &config);
int RunInteractive(const Config &config);

}
