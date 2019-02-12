// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

enum class MergeMode {
    Naive,
    CSS,
    JS
};

enum class SourceMapType {
    None,
    JSv3
};

struct SourceInfo {
    const char *filename;
    const char *name;

    const char *prefix;
    const char *suffix;
};
