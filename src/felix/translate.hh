// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"

namespace K {

struct TranslationMessage {
    const char *key;
    const char *value;
};

struct TranslationFile {
    const char *language;
    HeapArray<TranslationMessage> messages;
};

struct TranslationSet {
    HeapArray<TranslationFile> files;

    BlockAllocator str_alloc;
};

enum class TranslationFlag {
    NoSymbols = 1 << 0,
    NoArray = 1 << 1
};
static const char *const TranslationFlagNames[] = {
    "NoSymbols",
    "NoArray"
};

bool LoadTranslations(Span<const char *const> filenames, TranslationSet *out_set);
bool PackTranslations(Span<const TranslationFile> files, unsigned int flags, const char *output_filename);

}
