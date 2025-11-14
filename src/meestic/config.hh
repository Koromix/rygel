// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
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
