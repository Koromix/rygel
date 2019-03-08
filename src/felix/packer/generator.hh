// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "packer.hh"

enum class GeneratorType {
    C,
    Files
};
static const char *const GeneratorTypeNames[] = {
    "C",
    "Files"
};

bool GenerateC(Span<const AssetInfo> assets, const char *output_path,
               CompressionType compression_type);
bool GenerateFiles(Span<const AssetInfo> assets, const char *output_path,
                   CompressionType compression_type);
