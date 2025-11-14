// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

class gui_TrayIcon {
public:
    virtual ~gui_TrayIcon() = default;

    virtual bool SetIcon(Span<const uint8_t> png) = 0;

    virtual void OnActivation(std::function<void()> func) = 0;
    virtual void OnContext(std::function<void()> func) = 0;
#if defined(__linux__)
    virtual void OnScroll(std::function<void(int)> func) = 0;
#endif

    virtual void AddAction(const char *label, int check, std::function<void()> func) = 0;
    void AddAction(const char *label, std::function<void()> func) { AddAction(label, -1, func); }
    virtual void AddSeparator() = 0;
    virtual void ClearMenu() = 0;

    // You can skip this on Windows if you run the Win32 message pump for some reason
    virtual WaitSource GetWaitSource() = 0;
    virtual bool ProcessEvents() = 0;
};

std::unique_ptr<gui_TrayIcon> gui_CreateTrayIcon(Span<const uint8_t> png);

}
