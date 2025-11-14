// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "src/rekkord/lib/librekkord.hh"

namespace K {

extern const char *const DefaultConfigDirectory;
extern const char *const DefaultConfigName;
extern const char *const DefaultConfigEnv;

extern rk_Config rk_config;

void PrintCommonOptions(StreamWriter *st);
bool HandleCommonOption(OptionParser &opt, bool ignore_unknown = false);

}
