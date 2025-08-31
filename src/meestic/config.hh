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
#include "light.hh"

namespace K {

struct ConfigProfile {
    const char *name;

    bool manual = false;
    LightSettings settings;

    K_HASHTABLE_HANDLER(ConfigProfile, name);
};

struct Config {
    BucketArray<ConfigProfile> profiles;
    HashTable<const char *, const ConfigProfile *> profiles_map;

    Size default_idx = 0;

    BlockAllocator str_alloc;
};

struct PredefinedColor {
    const char *name;
    RgbColor rgb;
};

extern const Span<const PredefinedColor> PredefinedColors;

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

void AddDefaultProfiles(Config *out_config);

bool ParseColor(Span<const char> str, RgbColor *out_color);

}
