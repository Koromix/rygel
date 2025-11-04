// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"

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
