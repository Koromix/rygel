// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"

static void PrintAsHexArray(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    for (uint8_t byte: bytes) {
        Print(out_st, "%1, ", FmtHex(byte));
    }
}

static Size PackFile(const char *filename, CompressionType compression_type, StreamWriter *out_st)
{
    PrintLn(out_st, "    // %1", filename);
    Print(out_st, "    ");

    Size written_len = 0;
    {
        HeapArray<uint8_t> buf;
        StreamWriter writer(&buf, nullptr, compression_type);

        StreamReader reader(filename);
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
        writer.Close();
        written_len += buf.len;
        PrintAsHexArray(buf, out_st);
    }
    PrintLn(out_st);

    return written_len;
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: packer <filename> ...

Options:
    -O, --output <path>          Redirect output to file

    -d, --depth <depth>          Keep <depth> last components of filename
                                 (default: keep full path)
    -c, --compress <type>        Compress data, see below for available types
                                 (default: %1)
        --span_name <name>       Change span name exported by output code
                                 (default: packer_assets)

Available compression types:)", CompressionTypeNames[0]);
        for (const char *type: CompressionTypeNames) {
            PrintLn(fp, "    %1", type);
        }
    };

    OptionParser opt_parser(argc, argv);

    const char *output_path = nullptr;
    int depth = -1;
    const char *span_name = "packer_assets";
    CompressionType compression_type = CompressionType::None;
    HeapArray<const char *> filenames;
    {
        const char *opt;
        while ((opt = opt_parser.ConsumeOption())) {
            if (TestOption(opt, "--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (TestOption(opt, "-d", "--depth")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return 1;

                std::pair<int, bool> ret = ParseDec<int>(opt_parser.current_value);
                if (!ret.second)
                    return 1;
                if (ret.first <= 0) {
                    LogError("Option --depth requires value > 0");
                    return 1;
                }
                depth = ret.first;
            } else if (TestOption(opt, "--span_name")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return 1;

                span_name = opt_parser.current_value;
            } else if (TestOption(opt, "-O")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
                    return 1;

                output_path = opt_parser.current_value;
            } else if (TestOption(opt, "-c", "--compress")) {
                if (!opt_parser.RequireOptionValue(PrintUsage))
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
            } else {
                LogError("Unknown option '%1'", opt);
                return 1;
            }
        }

        opt_parser.ConsumeNonOptions(&filenames);
        if (!filenames.len) {
            LogError("No filename specified");
            PrintUsage(stderr);
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

#if defined(__x86_64__) || defined(_M_X64)
    typedef int64_t Size;
#elif defined(__i386__) || defined(_M_IX86) || defined(__EMSCRIPTEN__)
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

struct PackerAsset {
    const char *name;
    int compression_type; // CompressionType
    Span<const uint8_t> data;
};

static const uint8_t raw_data[] = {)");

    HeapArray<Size> lengths;
    for (const char *filename: filenames) {
        Size file_len = PackFile(filename, compression_type, &st);
        if (file_len < 0)
            return 1;
        lengths.Append(file_len);
    }

    PrintLn(&st,
R"(};

static PackerAsset assets[] = {)");

    for (Size i = 0, cumulative_len = 0; i < filenames.len; i++) {
        Span<const char> filename = filenames[i];

        Span<const char> name = filename;
        {
            Span<const char> remainder = filename;
            for (Size j = 0; remainder.len && j < depth; j++) {
                name = SplitStrReverseAny(remainder, PATH_SEPARATORS, &remainder);
            }
            name.len = filename.end() - name.ptr;
        }

        PrintLn(&st, "    {\"%1\", %2, {raw_data + %3, %4}},",
                name, (int)compression_type, cumulative_len, lengths[i]);
        cumulative_len += lengths[i];
    }

    PrintLn(&st,
R"(};

EXPORT extern const Span<const PackerAsset> %1;
const Span<const PackerAsset> %1 = assets;)", span_name);

    return 0;
}
