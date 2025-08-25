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

#include "src/core/base/base.hh"
#include "src/core/wrap/json.hh"
#include "translate.hh"

namespace RG {

// For simplicity, I've replicated the required data structures from core/base directly below.
// Don't forget to keep them in sync.
static const char *const CodePrefix =
R"(// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
typedef int64_t Size;
#elif defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(__EMSCRIPTEN__)
typedef int32_t Size;
#endif

#if defined(EXPORT)
    #if defined(_WIN32)
        #define EXPORT_SYMBOL __declspec(dllexport)
    #else
        #define EXPORT_SYMBOL __attribute__((visibility("default")))
    #endif
#else
    #define EXPORT_SYMBOL
#endif
#if defined(__cplusplus)
    #define EXTERN extern "C"
#else
    #define EXTERN extern
#endif

typedef struct Span {
    const void *ptr;
    Size len;
} Span;

typedef struct TranslationMessage {
    const char *key;
    const char *value;
} TranslationMessage;

typedef struct TranslationTable {
    const char *language;
    Span messages;
} TranslationTable;)";

static bool LoadStrings(json_Parser *parser, TranslationFile *out_file)
{
    parser->ParseObject();
    while (parser->InObject()) {
        TranslationMessage msg = {};

        parser->ParseKey(&msg.key);
        parser->SkipNull() || parser->ParseString(&msg.value);

        if (msg.value) {
            out_file->messages.Append(msg);
        }
    }

    return parser->IsValid();
}

bool LoadTranslations(Span<const char *const> filenames, TranslationSet *out_set)
{
    TranslationSet set;

    HashMap<Span<const char>, Size> map;

    for (const char *filename: filenames) {
        StreamReader reader(filename);
        if (!reader.IsValid())
            return false;
        json_Parser parser(&reader, &set.str_alloc);

        Span<const char> basename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);
        Span<const char> language = SplitStr(basename, '.');

        TranslationFile *file;
        {
            Size *ptr = map.TrySet(language, -1);
            Size idx = *ptr;

            if (idx >= 0) {
                file = &set.files[idx];
            } else {
                *ptr = set.files.len;

                file = set.files.AppendDefault();
                file->language = DuplicateString(language, &set.str_alloc).ptr;
            }
        }

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "keys") {
                // Not used in C++ code (yet?)
                parser.Skip();
            } else if (key == "messages") {
                if (!LoadStrings(&parser, file))
                    return false;
            } else if (parser.IsValid()) {
                LogError("Unexpected key '%1'", key);
                return false;
            }
        }
        if (!parser.IsValid())
            return false;
    }

    for (TranslationFile &file: set.files) {
        if (!file.messages.len)
            continue;

        std::sort(file.messages.begin(), file.messages.end(),
                  [](const TranslationMessage &msg1, const TranslationMessage &msg2) { return CmpStr(msg1.key, msg2.key) < 0; });

        Size j = 1;
        for (Size i = 1; i < file.messages.len; i++) {
            file.messages[j] = file.messages[i];
            j += !TestStr(file.messages[i - 1].key, file.messages[i].key);
        }
        file.messages.len = j;
    }

    std::swap(set, *out_set);
    return true;
}

bool PackTranslations(Span<const TranslationFile> files, unsigned int flags, const char *output_filename)
{
    StreamWriter c;
    if (output_filename) {
        if (!c.Open(output_filename))
            return false;
    } else {
        if (!c.Open(STDOUT_FILENO, "<stdout>"))
            return false;
    }

    PrintLn(&c, CodePrefix);

    for (Size i = 0; i < files.len; i++) {
        const TranslationFile &file = files[i];

        PrintLn(&c);
        PrintLn(&c, "static const TranslationMessage messages%1[] = {", i);
        for (const TranslationMessage &msg: file.messages) {
            PrintLn(&c, "    { \"%1\", \"%2\" },", FmtEscape(msg.key), FmtEscape(msg.value));
        }
        PrintLn(&c, "};");
    }

    if (!(flags & (int)TranslationFlag::NoArray)) {
        PrintLn(&c);

        PrintLn(&c, "EXPORT_SYMBOL EXTERN const Span TranslationTables;");
        if (files.len) {
            PrintLn(&c, "const TranslationTable tables[%1] = {", files.len);
            for (Size i = 0; i < files.len; i++) {
                const TranslationFile &file = files[i];
                PrintLn(&c, "    { \"%1\", { messages%2, %3 } },", file.language, i, file.messages.len);
            }
            PrintLn(&c, "};");
        }
        PrintLn(&c, "const Span TranslationTables = { tables, %1 };", files.len);
    }

    if (!(flags & (int)TranslationFlag::NoSymbols)) {
        PrintLn(&c);

        for (Size i = 0; i < files.len; i++) {
            const TranslationFile &file = files[i];

            PrintLn(&c, "EXPORT_SYMBOL EXTERN const TranslationTable TranslationTable%1;", FmtUpperAscii(file.language));
            PrintLn(&c, "const TranslationTable TranslationTable%1 = { \"%2\", { messages%3, %4 } };", FmtUpperAscii(file.language), file.language, i, file.messages.len);
        }
    }

    if (!c.Close())
        return false;

    return true;
}

}
