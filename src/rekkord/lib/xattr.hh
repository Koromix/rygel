// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

struct XAttrInfo {
    const char *key;
    Span<uint8_t> value;
};

bool ReadXAttributes(int fd, const char *filename, FileType type, Allocator *alloc, HeapArray<XAttrInfo> *out_xattrs);
bool WriteXAttributes(int fd, const char *filename, Span<const XAttrInfo> xattrs);

}
