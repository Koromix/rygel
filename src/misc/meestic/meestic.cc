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
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "lights.hh"
#include "vendor/libhs/libhs.h"

namespace RG {

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

static bool ParseColor(const char *str, RgbColor *out_color)
{
    static const HashMap<const char *, RgbColor> PredefinedColors {
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
    if (str[0] == '#') {
        Span<const char> remain = str + 1;

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

int Main(int argc, char **argv)
{
    // Options
    LightSettings settings;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options...] [colors...]%!0

Options:
    %!..+-m, --mode <mode>%!0            Set light mode (see below)
                                 %!D..(default: %2)%!0
    %!..+-s, --speed <speed>%!0          Set change of speed, from 0 and 2
                                 %!D..(default: %3)%!0
    %!..+-i, --intensity <intensity>%!0  Set light intensity, from 0 to 10
                                 %!D..(default: %4)%!0

Supported modes:)", FelixTarget, LightModeOptions[(int)settings.mode].name, settings.speed, settings.intensity);
        for (const OptionDesc &desc: LightModeOptions) {
            PrintLn(fp, "    %!..+%1%!0  %2", FmtArg(desc.name).Pad(27), desc.help);
        };
        PrintLn(fp, R"(
A few predefined color names can be used (such as MsiBlue), or you can use
hexadecimal RGB color codes. Don't forget the quotes or your shell may not
like the hash character.

Examples:
    Disable lighting
    %!..+%1 -m Disabled%!0

    Set default static MSI blue
    %!..+%1 -m Static MsiBlue%!0

    Slowly breathe between Orange and MsiBlue
    %!..+%1 -m Breathe -s 0 "#FFA100" MsiBlue%!0

    Quickly transition between Magenta, Orange and MsiBlue colors
    %!..+%1 -m Transition -s 2 Magenta Orange MsiBlue%!0

Be careful, color names and most options are %!..+case-sensitive%!0.)", FelixTarget);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Harmonize log output
    hs_log_set_handler([](hs_log_level level, int, const char *msg, void *) {
        switch (level) {
            case HS_LOG_ERROR:
            case HS_LOG_WARNING: { LogError("%1", msg); } break;
            case HS_LOG_DEBUG: { LogDebug("%1", msg); } break;
        }
    }, nullptr);

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-m", "--mode", OptionType::Value)) {
                if (!OptionToEnum(LightModeOptions, opt.current_value, &settings.mode)) {
                    LogError("Invalid mode '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-s", "--speed", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &settings.speed))
                    return 1;
            } else if (opt.Test("-i", "--intensity", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &settings.intensity))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        for (;;) {
            const char *arg = opt.ConsumeNonOption();
            if (!arg)
                break;

            RgbColor color;
            if (!ParseColor(arg, &color))
                return 1;

            if (!settings.colors.Available()) {
                LogError("A maximum of %1 colors is supported", RG_LEN(settings.colors.data));
                return 1;
            }
            settings.colors.Append(color);
        }
    }

    if (!ApplyLight(settings))
        return 1;

    LogInfo("Done!");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
