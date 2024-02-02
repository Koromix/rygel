// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/base/base.hh"
#include "src/core/wrap/json.hh"
#include "pack.hh"

namespace RG {

// For simplicity, I've replicated the required data structures from libcc
// and packer.hh directly below. Don't forget to keep them in sync.
static const char *const CodePrefix =
R"(// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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

#ifdef EXPORT
    #ifdef _WIN32
        #define EXPORT_SYMBOL __declspec(dllexport)
    #else
        #define EXPORT_SYMBOL __attribute__((visibility("default")))
    #endif
#else
    #define EXPORT_SYMBOL
#endif
#ifdef __cplusplus
    #define EXTERN_SYMBOL extern "C"
#else
    #define EXTERN_SYMBOL extern
#endif

typedef struct Span {
    const void *ptr;
    Size len;
} Span;

typedef struct AssetInfo {
    const char *name;
    int compression_type; // CompressionType
    Span data;
} AssetInfo;)";

enum class MergeMode {
    Naive,
    CSS,
    JS
};

struct MergeRule {
    const char *name;

    HeapArray<const char *> sources;

    bool override_compression;
    CompressionType compression_type;

    MergeMode merge_mode;
};

struct BlobInfo {
    const char *name;

    CompressionType compression_type;
    Size len;
};

static const char *StripDirectoryComponents(Span<const char> filename, int strip_count)
{
    const char *name = filename.ptr;
    for (int i = 0; filename.len && i <= strip_count; i++) {
        name = SplitStrAny(filename, RG_PATH_SEPARATORS, &filename).ptr;
    }

    return name;
}

static MergeMode FindDefaultMergeMode(const char *filename)
{
    Span<const char> extension = GetPathExtension(filename);

    if (extension == ".css") {
        return MergeMode::CSS;
    } else if (extension == ".js") {
        return MergeMode::JS;
    } else {
        return MergeMode::Naive;
    }
}

static bool LoadMergeRules(const char *filename, BlockAllocator *alloc, HeapArray<MergeRule> *out_rules)
{
    RG_DEFER_NC(out_guard, len = out_rules->len) { out_rules->RemoveFrom(len); };

    StreamReader st(filename);
    if (!st.IsValid())
        return false;

    IniParser ini(&st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            MergeRule *rule = out_rules->AppendDefault();
            rule->name = DuplicateString(prop.section, alloc).ptr;
            rule->merge_mode = FindDefaultMergeMode(rule->name);

            do {
                if (prop.key == "CompressionType") {
                    if (OptionToEnumI(CompressionTypeNames, prop.value, &rule->compression_type)) {
                        rule->override_compression = true;
                    } else {
                        LogError("Unknown compression type '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "MergeMode") {
                    if (prop.value == "Naive") {
                        rule->merge_mode = MergeMode::Naive;
                    } else if (prop.value == "CSS") {
                        rule->merge_mode = MergeMode::CSS;
                    } else if (prop.value == "JS") {
                        rule->merge_mode = MergeMode::JS;
                    } else {
                        LogError("Invalid MergeMode value '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "File") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, alloc).ptr;
                            rule->sources.Append(copy);
                        }
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    out_guard.Disable();
    return true;
}

static void InitSourceMergeData(PackSource *src, MergeMode merge_mode, Allocator *alloc)
{
    RG_ASSERT(alloc);

    switch (merge_mode) {
        case MergeMode::Naive: {
            src->prefix = "";
            src->suffix = "";
        } break;
        case MergeMode::CSS: {
            src->prefix = Fmt(alloc, "/* %1\n   ------------------------------------ */\n\n", src->filename).ptr;
            src->suffix = "\n";
        } break;
        case MergeMode::JS: {
            src->prefix = Fmt(alloc, "// %1\n// ------------------------------------\n\n", src->filename).ptr;
            src->suffix = "\n";
        } break;
    }
}

bool ResolveAssets(Span<const char *const> filenames, int strip_count,
                   CompressionType compression_type, PackAssetSet *out_set)
{
    // Reuse for performance
    HeapArray<MergeRule> rules;

    for (const char *filename: filenames) {
        if (StartsWith(filename, "@")) {
            rules.RemoveFrom(0);

            if (!LoadMergeRules(filename + 1, &out_set->str_alloc, &rules))
                return false;

            for (const MergeRule &rule: rules) {
                PackAsset *asset = out_set->assets.AppendDefault();

                asset->name = StripDirectoryComponents(rule.name, strip_count);
                asset->compression_type = rule.override_compression ? rule.compression_type : compression_type;

                for (const char *src_filename: rule.sources) {
                    PackSource *src = asset->sources.AppendDefault();

                    src->filename = src_filename;
                    InitSourceMergeData(src, rule.merge_mode, &out_set->str_alloc);
                }
            }
        } else {
            PackAsset *asset = out_set->assets.AppendDefault();
            PackSource *src = asset->sources.AppendDefault();

            asset->name = StripDirectoryComponents(filename, strip_count);
            asset->compression_type = compression_type;

            src->filename = filename;
        }
    }

    return true;
}

static bool MergeAssetSourceFiles(Span<const PackSource> sources, StreamWriter *writer)
{
    for (const PackSource &src: sources) {
        writer->Write(Span<const char>(src.prefix).As<const uint8_t>());

        StreamReader reader(src.filename);
        do {
            LocalArray<uint8_t, 16384> read_buf;
            read_buf.len = reader.Read(read_buf.data);
            if (read_buf.len < 0)
                return false;

            if (!writer->Write(read_buf))
                return false;
        } while (!reader.IsEOF());

        writer->Write(Span<const char>(src.suffix).As<const uint8_t>());
    }
    if (!writer->IsValid())
        return false;

    return true;
}

static Size WriteAsset(const PackAsset &asset, FunctionRef<void(Span<const uint8_t> buf)> func)
{
    Size compressed_len = 0;
    StreamWriter compressor([&](Span<const uint8_t> buf) { 
        func(buf);
        compressed_len += buf.len;

        return true;
    }, nullptr, asset.compression_type);

    if (!compressor.IsValid())
        return -1;

    if (!MergeAssetSourceFiles(asset.sources, &compressor))
        return -1;

    bool success = compressor.Close();
    RG_ASSERT(success);

    return compressed_len;
}

static void PrintAsLiterals(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    Size i = 0;
    for (Size end = bytes.len / 8 * 8; i < end; i += 8) {
        Print(out_st, "\"\\x%1\\x%2\\x%3\\x%4\\x%5\\x%6\\x%7\\x%8\" ",
                      FmtHex(bytes[i + 0]).Pad0(-2), FmtHex(bytes[i + 1]).Pad0(-2),
                      FmtHex(bytes[i + 2]).Pad0(-2), FmtHex(bytes[i + 3]).Pad0(-2),
                      FmtHex(bytes[i + 4]).Pad0(-2), FmtHex(bytes[i + 5]).Pad0(-2),
                      FmtHex(bytes[i + 6]).Pad0(-2), FmtHex(bytes[i + 7]).Pad0(-2));
    }

    if (i < bytes.len) {
        Print(out_st, "\"");
        for (; i < bytes.len; i++) {
            Print(out_st, "\\x%1", FmtHex(bytes[i]).Pad0(-2));
        }
        Print(out_st, "\" ");
    }
}

static void PrintAsArray(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    Size i = 0;
    for (Size end = bytes.len / 8 * 8; i < end; i += 8) {
        Print(out_st, "0x%1, 0x%2, 0x%3, 0x%4, 0x%5, 0x%6, 0x%7, 0x%8, ",
                      FmtHex(bytes[i + 0]).Pad0(-2), FmtHex(bytes[i + 1]).Pad0(-2),
                      FmtHex(bytes[i + 2]).Pad0(-2), FmtHex(bytes[i + 3]).Pad0(-2),
                      FmtHex(bytes[i + 4]).Pad0(-2), FmtHex(bytes[i + 5]).Pad0(-2),
                      FmtHex(bytes[i + 6]).Pad0(-2), FmtHex(bytes[i + 7]).Pad0(-2));
    }
    for (; i < bytes.len; i++) {
        Print(out_st, "0x%1, ", FmtHex(bytes[i]).Pad0(-2));
    }
}

static const char *MakeVariableName(const char *name, Allocator *alloc)
{
    HeapArray<char> buf(alloc);
    bool up = true;

    if (!IsAsciiAlpha(name[0]) && name[0] != '_') {
        buf.Append('_');
    }
    for (Size i = 0; name[i]; i++) {
        int c = name[i];

        if (IsAsciiAlphaOrDigit(c)) {
            buf.Append(up ? UpperAscii(c) : (char)c);
            up = false;
        } else {
            up = true;
        }
    }

    buf.Grow(1);
    buf.ptr[buf.len] = 0;

    return buf.Leak().ptr;
}

bool PackAssets(Span<const PackAsset> assets, unsigned int flags, const char *output_path)
{
    BlockAllocator temp_alloc;

    if ((flags & (int)PackFlag::UseEmbed) && (flags & (int)PackFlag::UseLiterals)) {
        LogError("Cannot use both UseEmbed and UseLiterals flags");
        return false;
    }

    StreamWriter c;
    StreamWriter bin;
    if (output_path) {
        if (!c.Open(output_path))
            return false;

        if (flags & (int)PackFlag::UseEmbed) {
            const char *bin_filename = Fmt(&temp_alloc, "%1.bin", output_path).ptr;

            if (!bin.Open(bin_filename))
                return false;
        }
    } else {
        if (flags & (int)PackFlag::UseEmbed) {
            LogError("You must use an explicit output path for UseEmbed");
            return false;
        }

        if (!c.Open(stdout, "<stdout>"))
            return false;
    }

    PrintLn(&c, CodePrefix);

    // Work around the ridiculousness of C++ not liking empty arrays
    HeapArray<BlobInfo> blobs;
    if (assets.len) {
        std::function<void(Span<const uint8_t>)> print;

        PrintLn(&c, R"(
static const uint8_t raw_data[] = {)");

        if (flags & (int)PackFlag::UseEmbed) {
            PrintLn(&c, "    #embed \"%1.bin\"", output_path);
            print = [&](Span<const uint8_t> buf) { bin.Write(buf); };
        } else if (flags & (int)PackFlag::UseLiterals) {
            print = [&](Span<const uint8_t> buf) { PrintAsLiterals(buf, &c); };
        } else {
            print = [&](Span<const uint8_t> buf) { PrintAsArray(buf, &c); };
        }

        // Pack assets and source maps
        for (const PackAsset &asset: assets) {
            BlobInfo blob = {};

            blob.name = asset.name;
            blob.compression_type = asset.compression_type;

            if (flags & (int)PackFlag::UseEmbed) {
                blob.len = WriteAsset(asset, print);
                if (blob.len < 0)
                    return false;

                // Put NUL byte at the end to make it a valid C string
                print(0);
            } else {
                PrintLn(&c, "    // %1", blob.name);
                Print(&c, "    ");
                blob.len = WriteAsset(asset, print);
                if (blob.len < 0)
                    return false;

                // Put NUL byte at the end to make it a valid C string
                print(0);
                PrintLn(&c);
            }

            blobs.Append(blob);
        }

        PrintLn(&c, "};");
    }

    if (!(flags & (int)PackFlag::NoArray)) {
        PrintLn(&c);

        PrintLn(&c, "EXPORT_SYMBOL EXTERN_SYMBOL const Span PackedAssets;");
        if (assets.len) {
            PrintLn(&c, "static AssetInfo assets[%1] = {", blobs.len);

            // Write asset table
            for (Size i = 0, raw_offset = 0; i < blobs.len; i++) {
                const BlobInfo &blob = blobs[i];

                PrintLn(&c, "    {\"%1\", %2, { raw_data + %3, %4 }},",
                             blob.name, (int)blob.compression_type, raw_offset, blob.len);

                raw_offset += blob.len + 1;
            }

            PrintLn(&c, "};");
        }
        PrintLn(&c, "const Span PackedAssets = {%1, %2};", blobs.len ? "assets" : "0", blobs.len);
    }

    if (!(flags & (int)PackFlag::NoSymbols)) {
        PrintLn(&c);

        for (Size i = 0, raw_offset = 0; i < blobs.len; i++) {
            const BlobInfo &blob = blobs[i];
            const char *var = MakeVariableName(blob.name, &temp_alloc);

            PrintLn(&c, "EXPORT_SYMBOL EXTERN_SYMBOL const AssetInfo %1;", var);
            PrintLn(&c, "const AssetInfo %1 = {\"%2\", %3, { raw_data + %4, %5 }};",
                         var, blob.name, (int)blob.compression_type, raw_offset, blob.len);

            raw_offset += blob.len + 1;
        }
    }

    if (!c.Close())
        return false;
    if ((flags & (int)PackFlag::UseEmbed) && !bin.Close())
        return false;

    return true;
}

}
