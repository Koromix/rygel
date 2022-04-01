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

#pragma pack(push, 1)
// Guessed through retro-engineering, each field is subject to interpretation
struct ControlPacket {
    int8_t report; // Report ID = 2
    char _pad1[1];
    int8_t mode; // 0 = disabled, 1 = static, 2 = breating, 5 = cycle
    int8_t speed; // 0 to 2
    int8_t intensity; // 1 to 10
    int8_t count; // 1 to 7 (there's space for more in the packet, I tried it but colors beyond 7 seem to be ignored)
    RgbColor colors[7];
    char _pad2[38];
};
RG_STATIC_ASSERT(RG_SIZE(ControlPacket) == 65);
#pragma pack(pop)

static void DumpPacket(Span<const uint8_t> bytes)
{
    PrintLn(stderr, "Length = %1:", FmtMemSize(bytes.len));

    for (const uint8_t *ptr = bytes.begin(); ptr < bytes.end();) {
        Print(stderr, "  [0x%1 %2 %3]  ", FmtArg(ptr).Pad0(-16),
                                          FmtArg((ptr - bytes.begin()) / sizeof(void *)).Pad(-4),
                                          FmtArg(ptr - bytes.begin()).Pad(-4));
        for (int i = 0; ptr < bytes.end() && i < (int)sizeof(void *); i++, ptr++) {
            Print(stderr, " %1", FmtHex(*ptr).Pad0(-2));
        }
        PrintLn(stderr);
    }
}

bool CheckLightSettings(const LightSettings &settings)
{
    bool valid = true;

    if (settings.intensity < 0 || settings.intensity > 10) {
        LogError("Intensity must be between 0 and 10");
        valid = false;
    }
    if (settings.speed < 0 || settings.speed > 2) {
        LogError("Speed must be between 0 and 2");
        valid = false;
    }
    if (settings.mode == LightMode::Disabled && settings.colors.len) {
        LogError("Cannot set any color in Disabled mode");
        valid = false;
    }
    if (settings.mode == LightMode::Static && settings.colors.len > 1) {
        LogError("Only one color is supported in Static mode");
        valid = false;
    }

    return valid;
}

hs_port *OpenLightDevice()
{
    hs_match_spec spec = HS_MATCH_TYPE_VID_PID(HS_DEVICE_TYPE_HID, 0x1462, 0x1564, nullptr);

    hs_device *dev;
    int ret = hs_find(&spec, 1, &dev);
    if (ret < 0)
        return nullptr;
    if (!ret) {
        LogError("Cannot find Mystic Light HID device (1462:1564)");
        return nullptr;
    }
    RG_DEFER { hs_device_unref(dev); };

    hs_port *port;
    if (hs_port_open(dev, HS_PORT_MODE_WRITE, &port) < 0)
        return nullptr;

    return port;
}

void CloseLightDevice(hs_port *port)
{
    hs_port_close(port);
}

bool ApplyLight(hs_port *port, const LightSettings &settings)
{
    if (!CheckLightSettings(settings))
        return false;

    ControlPacket pkt = {};

    pkt.report = 2;
    switch (settings.mode) {
        case LightMode::Disabled: { pkt.mode = 0; } break;
        case LightMode::Static: { pkt.mode = 1; } break;
        case LightMode::Breathe: { pkt.mode = 2; } break;
        case LightMode::Transition: { pkt.mode = 5; } break;
    }
    pkt.speed = (int8_t)settings.speed;
    pkt.intensity = (int8_t)settings.intensity;
    if (settings.colors.len) {
        pkt.count = (int8_t)settings.colors.len;
        memcpy_safe(pkt.colors, settings.colors.data, settings.colors.len * RG_SIZE(RgbColor));
    } else {
        pkt.count = 1;
        pkt.colors[0] = {29, 191, 255}; // MSI blue
    }

    if (GetDebugFlag("DUMP")) {
        DumpPacket(MakeSpan((const uint8_t *)&pkt, RG_SIZE(pkt)));
    }
    if (hs_hid_send_feature_report(port, (const uint8_t *)&pkt, RG_SIZE(pkt)) < 0)
        return false;

    return true;
}

bool ApplyLight(const LightSettings &settings)
{
    hs_port *port = OpenLightDevice();
    if (!port)
        return false;
    RG_DEFER { hs_port_close(port); };

    return ApplyLight(port, settings);
}

}
