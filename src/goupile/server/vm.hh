// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

class InstanceHolder;

enum class ZygoteResult {
    Parent,
    Child,
    Error
};

ZygoteResult RunZygote(bool sandbox, const char *view_directory);
void StopZygote();
bool CheckZygote();

Span<const char> MergeData(Span<const char> data, Span<const char> meta, Allocator *alloc);

}
