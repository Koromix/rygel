// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "vendor/pugixml/src/pugixml.hpp"

namespace K {

class xml_PugiWriter: public pugi::xml_writer {
    StreamWriter *st;

public:
    xml_PugiWriter(StreamWriter *st) : st(st) {}
    void write(const void* data, size_t size) override { st->Write((const uint8_t *)data, (Size)size); }
};

}
