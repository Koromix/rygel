// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"
#include "lights.hh"
#include "config.hh"

namespace RG {

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), &config.str_alloc);

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        const char *default_name = nullptr;

        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section.len) {
                ConfigProfile *profile = config.profiles.AppendDefault();
                profile->name = DuplicateString(prop.section, &config.str_alloc).ptr;

                if (!config.profiles_map.TrySet(profile).second) {
                    LogError("Duplicate profile name '%1'", profile->name);
                    valid = false;
                }

                do {
                    if (prop.key == "Mode") {
                        if (!OptionToEnum(LightModeOptions, prop.value, &profile->settings.mode)) {
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
                                        LogError("A maximum of %1 colors is supported", RG_LEN(profile->settings.colors.data));
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
                    config.default_idx = config.profiles.len - 1;
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

    if (!config.profiles.len) {
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
    static const HashMap<Span<const char>, RgbColor> PredefinedColors {
        {"LightGray",  {200, 200, 200}},
        {"Gray",       {130, 130, 130}},
        {"DarkDray",   {80, 80, 80}},
        {"Yellow",     {253, 249, 0}},
        {"Gold",       {255, 203, 0}},
        {"Orange",     {255, 161, 0}},
        {"Pink",       {255, 109, 194}},
        {"Red",        {230, 41, 55}},
        {"Maroon",     {190, 33, 55}},
        {"Green",      {0, 228, 48}},
        {"Lime",       {0, 158, 47}},
        {"DarkGreen",  {0, 117, 44}},
        {"MsiBlue",    {29, 191, 255}},
        {"SkyBlue",    {102, 191, 255}},
        {"Blue",       {0, 121, 241}},
        {"DarkBlue",   {0, 82, 172}},
        {"Purple",     {200, 122, 255}},
        {"Violet",     {135, 60, 190}},
        {"DarkPurple", {112, 31, 126}},
        {"Beige",      {211, 176, 131}},
        {"Brown",      {127, 106, 79}},
        {"DarkBrown",  {76, 63, 47}},
        {"White",      {255, 255, 255}},
        {"Magenta",    {255, 0, 255}}
    };

    // Try predefined colors first
    {
        const RgbColor *color = PredefinedColors.Find(str);

        if (color) {
            *out_color = *color;
            return true;
        }
    }

    // Parse hexadecimal color
    if (str.len && str[0] == '#') {
        Span<const char> remain = str.Take(1, str.len - 1);

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
