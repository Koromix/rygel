// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "../libdrd/libdrd.hh"

namespace K {

struct Config;

extern const char *const CommonOptions;

extern Config drdc_config;

bool HandleCommonOption(OptionParser &opt);

}
