// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"
#include "light.hh"
#include "src/tytools/libhs/libhs.h"

namespace K {

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
static_assert(K_SIZE(ControlPacket) == 65);
#pragma pack(pop)

static void DumpPacket(Span<const uint8_t> bytes)
{
    PrintLn(StdErr, T("Length = %1:"), FmtMemSize(bytes.len));

    for (const uint8_t *ptr = bytes.begin(); ptr < bytes.end();) {
        Print(StdErr, "  [0x%1 %2 %3]  ", FmtHex((uintptr_t)ptr, 16),
                                          FmtInt((ptr - bytes.begin()) / sizeof(void *), 4, ' '),
                                          FmtInt(ptr - bytes.begin(), 4, ' '));
        for (int i = 0; ptr < bytes.end() && i < (int)sizeof(void *); i++, ptr++) {
            Print(StdErr, " %1", FmtHex(*ptr, 2));
        }
        PrintLn(StdErr);
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
    hs_match_spec specs[] = {
        HS_MATCH_TYPE_VID_PID(HS_DEVICE_TYPE_HID, 0x1462, 0x1562, nullptr),
        HS_MATCH_TYPE_VID_PID(HS_DEVICE_TYPE_HID, 0x1462, 0x1563, nullptr),
        HS_MATCH_TYPE_VID_PID(HS_DEVICE_TYPE_HID, 0x1462, 0x1564, nullptr)
    };

    hs_device *dev;
    int ret = hs_find(specs, K_LEN(specs), &dev);
    if (ret < 0)
        return nullptr;
    if (!ret) {
        LogError("Cannot find Mystic Light HID device (1462:1562, 1462:1563 or 1462:1564)");
        return nullptr;
    }
    K_DEFER { hs_device_unref(dev); };

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
        MemCpy(pkt.colors, settings.colors.data, settings.colors.len * K_SIZE(RgbColor));
    } else {
        pkt.count = 1;
        pkt.colors[0] = { 29, 191, 255 }; // MSI blue
    }

    if (GetDebugFlag("DUMP_PACKETS")) {
        DumpPacket(MakeSpan((const uint8_t *)&pkt, K_SIZE(pkt)));
    }
    if (hs_hid_send_feature_report(port, (const uint8_t *)&pkt, K_SIZE(pkt)) < 0)
        return false;

    return true;
}

bool ApplyLight(const LightSettings &settings)
{
    hs_port *port = OpenLightDevice();
    if (!port)
        return false;
    K_DEFER { CloseLightDevice(port); };

    return ApplyLight(port, settings);
}

}
