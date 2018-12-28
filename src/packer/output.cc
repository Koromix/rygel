// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/json.hh"

#include "output.hh"

static void PrintAsHexArray(Span<const uint8_t> bytes, StreamWriter *out_st)
{
    Size i = 0;
    for (Size end = bytes.len / 8 * 8; i < end; i += 8) {
        Print(out_st, "0x%1, 0x%2, 0x%3, 0x%4, 0x%5, 0x%6, 0x%7, 0x%8,",
              FmtHex(bytes[i + 0]).Pad0(-2), FmtHex(bytes[i + 1]).Pad0(-2),
              FmtHex(bytes[i + 2]).Pad0(-2), FmtHex(bytes[i + 3]).Pad0(-2),
              FmtHex(bytes[i + 4]).Pad0(-2), FmtHex(bytes[i + 5]).Pad0(-2),
              FmtHex(bytes[i + 6]).Pad0(-2), FmtHex(bytes[i + 7]).Pad0(-2));
    }
    for (; i < bytes.len; i++) {
        Print(out_st, "0x%1, ", FmtHex(bytes[i]).Pad0(-2));
    }
}

static FmtArg FormatZigzagVLQ64(int value)
{
    DebugAssert(value != INT_MIN);

    static const char literals[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    StaticAssert(SIZE(literals) - 1 == 64);
    StaticAssert((SIZE(FmtArg::value.str_buf) - 1) * 6 - 2 >= SIZE(value) * 16);

    FmtArg arg;
    arg.type = FmtArg::Type::StrBuf;

    // First character
    unsigned int u;
    if (value >= 0) {
        u = value >> 4;
        arg.value.str_buf[0] = literals[(value & 0xF) << 1 | (u ? 0x20 : 0)];
    } else {
        value = -value;
        u = value >> 4;
        arg.value.str_buf[0] = literals[((value & 0xF) << 1) | (u ? 0x21 : 0x1)];
    }

    // Remaining characters
    Size len = 1;
    while (u) {
        unsigned int idx = u & 0x1F;
        u >>= 5;
        arg.value.str_buf[len++] = literals[idx | (u ? 0x20 : 0)];
    }

    arg.value.str_buf[len] = 0;

    return arg;
}

static Size CountNewLines(Span<const char> buf)
{
    Size lines = 0;
    for (Size i = 0;; lines++) {
        const char *ptr = (const char *)memchr(buf.ptr + i, '\n', buf.len - i);
        if (!ptr)
            break;
        i = ptr - buf.ptr + 1;
    }

    return lines;
}

static bool BuildJavaScriptMap3(Span<const SourceInfo> sources, StreamWriter *out_writer)
{
    JsonStreamWriter json_st(out_writer);
    rapidjson::Writer<JsonStreamWriter> writer(json_st);

    writer.StartObject();

    writer.Key("version"); writer.Int(3);
    writer.Key("sources"); writer.StartArray();
    for (const SourceInfo &src: sources) {
        writer.String(src.name);
    }
    writer.EndArray();
    writer.Key("names"); writer.StartArray(); writer.EndArray();

    writer.Key("mappings"); json_st.Flush(); out_writer->Write(":\"");
    for (Size i = 0, prev_lines = 0; i < sources.len; i++) {
        const SourceInfo &src = sources[i];

        Size lines = 0;
        {
            StreamReader reader(src.filename);
            while (!reader.eof) {
                char buf[128 * 1024];
                Size len = reader.Read(SIZE(buf), &buf);
                if (len < 0)
                    return false;

                lines += CountNewLines(MakeSpan(buf, len));
            }
        }

        Print(out_writer, "%1", FmtArg(";").Repeat(CountNewLines(src.prefix)));
        if (lines) {
            Print(out_writer, "A%1%2A;", i ? "C" : "A", FormatZigzagVLQ64(-prev_lines));
            lines--;

            for (Size j = 0; j < lines; j++) {
                Print(out_writer, "AACA;");
            }
        }
        Print(out_writer, "%1", FmtArg(";").Repeat(CountNewLines(src.suffix)));

        prev_lines = lines;
    }
    out_writer->Write('"');

    writer.EndObject();

    return true;
}

Size PackAsset(Span<const SourceInfo> sources,
               CompressionType compression_type, StreamWriter *out_st)
{
    Size written_len = 0;
    {
        HeapArray<uint8_t> buf;
        StreamWriter writer(&buf, nullptr, compression_type);

        const auto flush_buffer = [&]() {
            written_len += buf.len;
            PrintAsHexArray(buf, out_st);
            buf.RemoveFrom(0);
        };

        for (const SourceInfo &src: sources) {
            writer.Write(src.prefix);

            StreamReader reader(src.filename);
            while (!reader.eof) {
                uint8_t read_buf[128 * 1024];
                Size read_len = reader.Read(SIZE(read_buf), read_buf);
                if (read_len < 0)
                    return false;

                Assert(writer.Write(read_buf, read_len));
                flush_buffer();
            }

            writer.Write(src.suffix);
        }

        Assert(writer.Close());
        flush_buffer();
    }

    return written_len;
}

Size PackSourceMap(Span<const SourceInfo> sources, SourceMapType source_map_type,
                   CompressionType compression_type, StreamWriter *out_st)
{
    HeapArray<uint8_t> buf;
    StreamWriter writer(&buf, nullptr, compression_type);

    switch (source_map_type) {
        case SourceMapType::None: {} break;
        case SourceMapType::JSv3: {
            if (!BuildJavaScriptMap3(sources, &writer))
                return false;
        } break;
    }

    Assert(writer.Close());
    PrintAsHexArray(buf, out_st);

    return buf.len;
}
