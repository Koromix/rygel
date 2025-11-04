// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#if defined(__APPLE__)

#include "src/core/native/base/base.hh"

namespace K {

struct MacBundleSettings {
    const char *title = nullptr;
    const char *icon_filename = nullptr;
    bool force = false;
};

bool BundleMacBinary(const char *binary_filename, const char *output_dir, const MacBundleSettings &settings);

}

#endif
