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

#if defined(__APPLE__)

#include "src/core/base/base.hh"

namespace RG {

struct MacBundleSettings {
    const char *title = nullptr;
    const char *icon_filename = nullptr;
    bool force = false;
};

bool BundleMacBinary(const char *binary_filename, const char *output_dir, const MacBundleSettings &settings);

}

#endif
