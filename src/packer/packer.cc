// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"

struct SourceInfo {
    const char *prefix;
    const char *suffix;
    const char *filename;
};

struct PackFileInfo {
    const char *filename;
    HeapArray<SourceInfo> sources;
};

static void PrintAsHexArray(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    for (uint8_t byte: bytes) {
        Print(out_st, "0x%1, ", FmtHex(byte).Pad0(-2));
    }
}

static Size PackFile(const char *filename, Span<const SourceInfo> sources,
                     CompressionType compression_type, StreamWriter *out_st)
{
    PrintLn(out_st, "    // %1", filename);
    Print(out_st, "    ");

    Size written_len = 0;
    {
        HeapArray<uint8_t> buf;
        StreamWriter writer(&buf, nullptr, compression_type);
        for (const SourceInfo &src: sources) {
            if (src.prefix) {
                writer.Write(src.prefix);
            }

            StreamReader reader(src.filename);
            while (!reader.eof) {
                uint8_t read_buf[128 * 1024];
                Size read_len = reader.Read(SIZE(read_buf), read_buf);
                if (read_len < 0)
                    return false;

                writer.Write(read_buf, read_len);
                written_len += buf.len;
                PrintAsHexArray(buf, out_st);
                buf.RemoveFrom(0);
            }

            if (src.suffix) {
                writer.Write(src.suffix);
            }
        }
        writer.Close();
        written_len += buf.len;
        PrintAsHexArray(buf, out_st);
    }
    PrintLn(out_st);

    return written_len;
}

static SourceInfo CreateSource(const char *filename, const char *extension, bool merge, Allocator *alloc)
{
    SourceInfo src = {};

    src.filename = filename;
    if (merge) {
        if (TestStr(extension, ".css")) {
            src.prefix = Fmt(alloc, "/* %1\n   ------------------------------------ */\n\n", filename).ptr;
            src.suffix = "\n";
        } else if (TestStr(extension, ".js")) {
            src.prefix = Fmt(alloc, "// %1\n// ------------------------------------\n\n", filename).ptr;
            src.suffix = "\n";
        } else {
            LogError("Doing naive merge for '%1' files", extension);
        }
    }

    return src;
}

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc(Kibibytes(8));

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: packer <filename> ...

Options:
    -O, --output_file <file>     Redirect output to file

    -d, --depth <depth>          Keep <depth> last components of filename
                                 (default: keep full path)
    -c, --compress <type>        Compress data, see below for available types
                                 (default: %1)
        --span_name <name>       Change span name exported by output code
                                 (default: packer_assets)
    -e, --export                 Export span symbol

    -M, --merge_name <name>      Base name for merged files
                                 (default: packed)
    -m, --merge <extensions>     Merge files with extensions together

Available compression types:)", CompressionTypeNames[0]);
        for (const char *type: CompressionTypeNames) {
            PrintLn(fp, "    %1", type);
        }
    };


    const char *output_path = nullptr;
    int depth = -1;
    const char *span_name = "packer_assets";
    bool export_span = false;
    CompressionType compression_type = CompressionType::None;
    HashSet<const char *> merge_extensions;
    const char *merge_name = "packed";
    HeapArray<const char *> filenames;
    {
        OptionParser opt_parser(argc, argv);

        while (opt_parser.Next()) {
            if (opt_parser.TestOption("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt_parser.TestOption("-d", "--depth")) {
                if (!opt_parser.RequireValue())
                    return 1;

                if (!ParseDec<int>(opt_parser.current_value, &depth))
                    return 1;
                if (depth <= 0) {
                    LogError("Option --depth requires value > 0");
                    return 1;
                }
            } else if (opt_parser.TestOption("--span_name")) {
                span_name = opt_parser.RequireValue();
                if (!span_name)
                    return 1;
            } else if (opt_parser.TestOption("--export", "-e")) {
                export_span = true;
            } else if (opt_parser.TestOption("-O", "--output_file")) {
                output_path = opt_parser.RequireValue();
                if (!output_path)
                    return 1;
            } else if (opt_parser.TestOption("-c", "--compress")) {
                if (!opt_parser.RequireValue())
                    return 1;

                Size i = 0;
                for (; i < ARRAY_SIZE(CompressionTypeNames); i++) {
                    if (TestStrI(CompressionTypeNames[i], opt_parser.current_value))
                        break;
                }
                if (i >= ARRAY_SIZE(CompressionTypeNames)) {
                    LogError("Unknown compression type '%1'", opt_parser.current_value);
                    return 1;
                }

                compression_type = (CompressionType)i;
            } else if (opt_parser.TestOption("-m", "--merge")) {
                if (!opt_parser.RequireValue())
                    return 1;

                Span<const char> remain = opt_parser.current_value;
                while (remain.len) {
                    Span<const char> part = TrimStr(SplitStrAny(remain, ", ", &remain));
                    if (part.len) {
                        const char *extension = Fmt(&temp_alloc, ".%1", part).ptr;
                        merge_extensions.Append(extension);
                    }
                }
            } else if (opt_parser.TestOption("-M", "--merge_name")) {
                merge_name = opt_parser.RequireValue();
                if (!merge_name)
                    return 1;
            } else {
                LogError("Unknown option '%1'", opt_parser.current_option);
                return 1;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
        if (!filenames.len) {
            LogError("No filename specified");
            return 1;
        }
    }

    StreamWriter st;
    if (output_path) {
        st.Open(output_path);
    } else {
        st.Open(stdout, nullptr);
    }
    if (st.error)
        return 1;

    // For simplicity, I've replicated the required data structures from kutil.hh
    // and packer.hh directly below. Don't forget to keep them in sync.
    PrintLn(&st,
R"(// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <initializer_list>
#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
    typedef int64_t Size;
#elif defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(__EMSCRIPTEN__)
    typedef int32_t Size;
#endif

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

template <typename T>
struct Span {
    T *ptr;
    Size len;

    Span() = default;
    constexpr Span(T *ptr_, Size len_) : ptr(ptr_), len(len_) {}
    template <Size N>
    constexpr Span(T (&arr)[N]) : ptr(arr), len(N) {}
};

enum class CompressionType {
    None,
    Zlib,
    Gzip
};

struct PackerAsset {
    const char *name;
    CompressionType compression_type;
    Span<const uint8_t> data;
};

static const uint8_t raw_data[] = {)");

    HeapArray<PackFileInfo> files;
    {
        HashMap<const char *, Size> merge_map;
        for (const char *filename: filenames) {
            const char *extension = GetPathExtension(filename).ptr;

            Size file_idx = merge_map.FindValue(extension, -1);
            bool merge;
            if (file_idx >= 0) {
                merge = true;
            } else if (merge_extensions.Find(extension)) {
                file_idx = files.len;

                PackFileInfo file = {};
                file.filename = Fmt(&temp_alloc, "%1%2", merge_name, extension).ptr;
                files.Append(file);

                merge_map.Append(extension, file_idx);
                merge = true;
            } else {
                file_idx = files.len;

                PackFileInfo file = {};
                file.filename = filename;
                files.Append(file);

                merge = false;
            }

            SourceInfo src = CreateSource(filename, extension, merge, &temp_alloc);
            files[file_idx].sources.Append(src);
        }
    }

    HeapArray<Size> lengths;
    for (const PackFileInfo &file: files) {
        Size file_len = PackFile(file.filename, file.sources, compression_type, &st);
        if (file_len < 0)
            return 1;
        lengths.Append(file_len);
    }

    PrintLn(&st,
R"(};

static PackerAsset assets[] = {)");

    for (Size i = 0, cumulative_len = 0; i < files.len; i++) {
        Span<const char> filename = files[i].filename;

        Span<const char> name = filename;
        {
            Span<const char> remainder = filename;
            for (Size j = 0; remainder.len && j < depth; j++) {
                name = SplitStrReverseAny(remainder, PATH_SEPARATORS, &remainder);
            }
            name.len = filename.end() - name.ptr;
        }

        PrintLn(&st, "    {\"%1\", (CompressionType)%2, {raw_data + %3, %4}},",
                name, (int)compression_type, cumulative_len, lengths[i]);
        cumulative_len += lengths[i];
    }

    PrintLn(&st,
R"(};

%1extern const Span<const PackerAsset> %2;
const Span<const PackerAsset> %2 = assets;)", export_span ? "EXPORT " : "", span_name);

    return 0;
}
