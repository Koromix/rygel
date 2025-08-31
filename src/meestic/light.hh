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

struct hs_port;

namespace K {

struct RgbColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

enum class LightMode {
    Disabled,
    Static,
    Breathe,
    Transition
};
static const OptionDesc LightModeOptions[] = {
    { "Disabled",   T("Disable keyboard light") },
    { "Static",     T("Use static lighting") },
    { "Breathe",    T("Breathe each color") },
    { "Transition", T("Transition between colors") }
};

struct LightSettings {
    LightMode mode = LightMode::Static;
    int speed = 0;
    int intensity = 10;
    LocalArray<RgbColor, 7> colors;
};

bool CheckLightSettings(const LightSettings &settings);

hs_port *OpenLightDevice();
void CloseLightDevice(hs_port *port);

bool ApplyLight(hs_port *port, const LightSettings &settings);
bool ApplyLight(const LightSettings &settings);

}
