// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "light.hh"
#include "config.hh"

namespace K {

static const PredefinedColor ColorTable[] = {
    { "LightGray",  { 200, 200, 200 } },
    { "Gray",       { 130, 130, 130 } },
    { "DarkDray",   { 80, 80, 80 } },
    { "Yellow",     { 253, 249, 0 } },
    { "Gold",       { 255, 203, 0 } },
    { "Orange",     { 255, 161, 0 } },
    { "Pink",       { 255, 109, 194 } },
    { "Red",        { 230, 41, 55 } },
    { "Maroon",     { 190, 33, 55 } },
    { "Green",      { 0, 228, 48 } },
    { "Lime",       { 0, 158, 47 } },
    { "DarkGreen",  { 0, 117, 44 } },
    { "MsiBlue",    { 29, 191, 255 } },
    { "SkyBlue",    { 102, 191, 255 } },
    { "Blue",       { 0, 121, 241 } },
    { "DarkBlue",   { 0, 82, 172 } },
    { "Purple",     { 200, 122, 255 } },
    { "Violet",     { 135, 60, 190 } },
    { "DarkPurple", { 112, 31, 126 } },
    { "Beige",      { 211, 176, 131 } },
    { "Brown",      { 127, 106, 79 } },
    { "DarkBrown",  { 76, 63, 47 } },
    { "White",      { 255, 255, 255 } },
    { "Magenta",    { 255, 0, 255 } }
};
const Span<const PredefinedColor> PredefinedColors = ColorTable;

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), &config.str_alloc);

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        const char *default_name = nullptr;

        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section.len) {
                ConfigProfile *profile = config.profiles.AppendDefault();
                profile->name = DuplicateString(prop.section, &config.str_alloc).ptr;

                bool inserted;
                config.profiles_map.TrySet(profile, &inserted);

                if (!inserted) {
                    LogError("Duplicate profile name '%1'", profile->name);
                    valid = false;
                }

                do {
                    if (prop.key == "Mode") {
                        if (!OptionToEnumI(LightModeOptions, prop.value, &profile->settings.mode)) {
                            LogError("Invalid mode '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "Speed") {
                        valid &= ParseInt(prop.value, &profile->settings.speed);
                    } else if (prop.key == "Intensity") {
                        valid &= ParseInt(prop.value, &profile->settings.intensity);
                    } else if (prop.key == "Colors") {
                        profile->settings.colors.Clear();

                        while (prop.value.len) {
                            Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));

                            if (part.len) {
                                RgbColor color = {};

                                if (ParseColor(part, &color)) {
                                    if (!profile->settings.colors.Available()) {
                                        LogError("A maximum of %1 colors is supported", K_LEN(profile->settings.colors.data));
                                        valid = false;
                                        break;
                                    }

                                    profile->settings.colors.Append(color);
                                } else {
                                    valid = false;
                                }
                            }
                        }
                    } else if (prop.key == "ManualOnly") {
                        valid &= ParseBool(prop.value, &profile->manual);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));

                valid &= CheckLightSettings(profile->settings);

                if (default_name && TestStr(default_name, profile->name)) {
                    config.default_idx = config.profiles.count - 1;
                    default_name = nullptr;
                }
            } else {
                do {
                    if (prop.key == "Default") {
                        default_name = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            }
        }

        if (default_name) {
            LogError("Default profile %1 does not exist", default_name);
            valid = false;
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    if (!config.profiles.count) {
        LogError("Config file contains no profile");
        return false;
    }
    if (std::all_of(config.profiles.begin(), config.profiles.end(),
                    [](const ConfigProfile &profile) { return profile.manual; })) {
        LogError("At least one profile must use Manual = Off");
        return false;
    }

    std::swap(*out_config, config);
    return true;
}

bool LoadConfig(const char *filename, Config *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

static inline ConfigProfile MakeDefaultProfile(const char *name, LightMode mode)
{
    ConfigProfile profile;

    profile.name = name;
    profile.settings.mode = mode;

    return profile;
}

void AddDefaultProfiles(Config *out_config)
{
    out_config->profiles.Append(MakeDefaultProfile(T("Enable"), LightMode::Static));
    out_config->profiles.Append(MakeDefaultProfile(T("Disable"), LightMode::Disabled));
}

static inline int ParseHexadecimalChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

bool ParseColor(Span<const char> str, RgbColor *out_color)
{
    // Try predefined colors first
    {
        const PredefinedColor *color = std::find_if(PredefinedColors.begin(), PredefinedColors.end(),
                                                    [&](const PredefinedColor &color) { return TestStr(color.name, str); });

        if (color != PredefinedColors.end()) {
            *out_color = color->rgb;
            return true;
        }
    }

    // Parse hexadecimal color
    {
        Span<const char> remain = str;

        if (remain.len && remain[0] == '#') {
            remain.ptr++;
            remain.len--;
        }

        if (remain.len != 6 || !std::all_of(remain.begin(), remain.end(), [](char c) { return ParseHexadecimalChar(c) >= 0; })) {
            LogError("Malformed hexadecimal color");
            return false;
        }

        out_color->red = (uint8_t)((ParseHexadecimalChar(remain[0]) << 4) | ParseHexadecimalChar(remain[1]));
        out_color->green = (uint8_t)((ParseHexadecimalChar(remain[2]) << 4) | ParseHexadecimalChar(remain[3]));
        out_color->blue = (uint8_t)((ParseHexadecimalChar(remain[4]) << 4) | ParseHexadecimalChar(remain[5]));

        return true;
    }

    LogError("Unknown color '%1'", str);
    return false;
}

}
