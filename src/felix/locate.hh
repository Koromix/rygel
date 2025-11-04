// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"

namespace K {

class Compiler;

struct QtInfo {
    const char *qmake = nullptr;
    const char *moc = nullptr;
    const char *rcc = nullptr;
    const char *uic = nullptr;
#if defined(__APPLE__)
    const char *macdeployqt = nullptr;
#endif

    const char *binaries = nullptr;
    const char *headers = nullptr;
    const char *libraries = nullptr;
    const char *plugins = nullptr;

    int64_t version = 0;
    int version_major = 0;
    bool shared = false;
};

struct WasiSdkInfo {
    const char *path = nullptr;
    const char *cc = nullptr;
    const char *sysroot = nullptr;
};

const QtInfo *FindQtSdk(const Compiler *compiler);
const WasiSdkInfo *FindWasiSdk();
const char *FindArduinoSdk();

}
