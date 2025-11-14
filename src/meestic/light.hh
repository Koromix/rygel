// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

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
