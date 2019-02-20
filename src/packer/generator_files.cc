// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "generator.hh"
#include "packer.hh"

bool GenerateFiles(Span<const AssetInfo> assets, const char *output_path,
                   CompressionType compression_type)
{
    LogError("Generator 'Files' does not work yet");
    return false;
}
