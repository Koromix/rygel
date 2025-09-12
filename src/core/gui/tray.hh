// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "src/core/base/base.hh"

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
};

std::unique_ptr<gui_TrayIcon> gui_CreateTrayIcon(Span<const uint8_t> png);

}
