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

#include "src/core/libcc/libcc.hh"
#include "src/core/libnet/libnet.hh"

namespace RG {

struct HttpHeader {
    const char *key;
    const char *value;
};

struct Config {
    http_Config http { 80 };

    const char *root_directory = ".";
    bool auto_index = true;
    HeapArray<HttpHeader> headers;

    bool set_etag = true;
    int max_age = 0;

    BlockAllocator str_alloc;

    bool Validate() const;
};

static Config config;

bool Config::Validate() const
{
    bool valid = true;

    valid &= http.Validate();
    if (max_age < 0) {
        LogError("HTTP MaxAge must be >= 0");
        valid = false;
    }

    return valid;
}

static bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), &config.str_alloc);

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "HTTP") {
                do {
                    valid &= config.http.SetProperty(prop.key.ptr, prop.value.ptr, root_directory);
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Files") {
                do {
                    if (prop.key == "RootDirectory") {
                        config.root_directory = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "AutoIndex") {
                        valid &= ParseBool(prop.value, &config.auto_index);
                    } else if (prop.key == "MaxAge") {
                        valid &= ParseInt(prop.value, &config.max_age);
                    } else if (prop.key == "ETag") {
                        valid &= ParseBool(prop.value, &config.set_etag);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Headers") {
                do {
                    HttpHeader header = {};

                    header.key = DuplicateString(prop.key, &config.str_alloc).ptr;
                    header.value = DuplicateString(prop.value, &config.str_alloc).ptr;

                    config.headers.Append(header);
                } while (ini.NextInSection(&prop));
            } else {
                LogError("Unknown section '%1'", prop.section);
                while (ini.NextInSection(&prop));
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    // Default values
    if (!config.Validate())
        return false;

    std::swap(*out_config, config);
    return true;
}

static bool LoadConfig(const char *filename, Config *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

static void ServeFile(const char *filename, const FileInfo &file_info, const http_RequestInfo &request, http_IO *io)
{
    const char *etag = config.set_etag ? Fmt(&io->allocator, "%1-%2", file_info.mtime, file_info.size).ptr : nullptr;

    // Handle ETag caching
    if (etag) {
        const char *client_etag = request.GetHeaderValue("If-None-Match");

        if (client_etag && TestStr(client_etag, etag)) {
            MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            io->AttachResponse(304, response);
            return;
        }
    }

    // Send the file
    const char *mimetype = http_GetMimeType(GetPathExtension(filename));
    io->AttachFile(200, filename, mimetype);
    io->AddCachingHeaders(config.max_age, etag);
}

static void WriteContent(Span<const char> str, StreamWriter *writer) {
    for (char c: str) {
        if (c == '&') {
            writer->Write("&amp;");
        } else if (c == '<') {
            writer->Write("&lt;");
        } else if (c == '>') {
            writer->Write("&gt;");
        } else if ((uint8_t)c < 32) {
            Print(writer, "<0x%1>", FmtHex((uint8_t)c).Pad0(-2));
        } else {
            writer->Write(c);
        }
    }
}

static void WriteURL(Span<const char> str, StreamWriter *writer) {
    for (char c: str) {
        if (IsAsciiAlphaOrDigit(c) || c == '/' || c == '-' || c == '.' || c == '_' || c == '~') {
            writer->Write((char)c);
        } else {
            Print(writer, "%%%1", FmtHex((uint8_t)c).Pad0(-2));
        }
    }
}

static void ServeIndex(const char *dirname, const http_RequestInfo &request, http_IO *io)
{
    static Span<const char> IndexTemplate =
R"(<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8"/>
        <title>{{ TITLE }}</title>
    </head>
    <body>
{{ CONTENT }}
    </body>
</html>
)";

    io->RunAsync([=]() {
        HeapArray<Span<const char>> names;
        {
            EnumResult ret = EnumerateDirectory(dirname, nullptr, 4096,
                                                [&](const char *basename, FileType file_type) {
                Span<const char> filename = Fmt(&io->allocator, "%1%2", basename, file_type == FileType::Directory ? "/" : "");
                names.Append(filename);

                return true;
            });

            if (ret != EnumResult::Success) {
                switch (ret) {
                    case EnumResult::Success: { RG_UNREACHABLE(); } break;

                    case EnumResult::MissingPath: { io->AttachError(404); } break;
                    case EnumResult::AccessDenied: { io->AttachError(403); } break;
                    case EnumResult::PartialEnum: {
                        LogError("Too many files");
                        io->AttachError(413);
                    } break;
                    case EnumResult::CallbackFail:
                    case EnumResult::OtherError: { /* 500 */ } break;
                }

                return;
            }
        }

        std::sort(names.begin(), names.end(), [](Span<const char> name1, Span<const char> name2) {
            bool is_directory1 = EndsWith(name1, "/");
            bool is_directory2 = EndsWith(name2, "/");

            if (is_directory1 != is_directory2) {
                return is_directory1;
            } else {
                return CmpStr(name1, name2) < 0;
            }
        });

        StreamWriter st;
        if (!io->OpenForWrite(200, -1, &st))
            return;

        PatchFile(IndexTemplate.As<const uint8_t>(), &st, [&](Span<const char> expr, StreamWriter *writer) {
            Span<const char> key = TrimStr(expr);

            if (key == "TITLE") {
                Span<const char> title = Fmt(&io->allocator, "%1/", SplitStrReverseAny(dirname, RG_PATH_SEPARATORS));
                WriteContent(title, writer);
            } else if (key == "CONTENT") {
                if (!TestStr(request.url, "/")) {
                    writer->Write("        <a href=\"..\">Go back</a>");
                }

                if (names.len) {
                    writer->Write("        <ul>\n");
                    for (Span<const char> name: names) {
                        writer->Write("            <li><a href=\""); WriteURL(name, writer); writer->Write("\">");
                        WriteContent(name, writer); writer->Write("</a></li>\n");
                    }
                    writer->Write("        </ul>");
                } else {
                    writer->Write("<p>Empty directory</p>");
                }
            } else {
                Print(writer, "{{%1}}", expr);
            }
        });
    });
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
    RG_ASSERT(StartsWith(request.url, "/"));

    // Security check
    if (PathContainsDotDot(request.url)) {
        LogError("Unsafe URL containing '..' components");
        io->AttachError(403);
        return;
    }

    // Add configured headers
    for (const HttpHeader &header: config.headers) {
        io->AddHeader(header.key, header.value);
    }

    const char *url = TrimStrLeft(request.url, "/\\").ptr;
    const char *filename = NormalizePath(url, config.root_directory, &io->allocator).ptr;

    FileInfo file_info;
    {
        StatResult stat = StatFile(filename, &file_info);

        if (stat != StatResult::Success) {
            switch (stat) {
                case StatResult::Success: { RG_UNREACHABLE(); } break;

                case StatResult::MissingPath: { io->AttachError(404); } break;
                case StatResult::AccessDenied: { io->AttachError(403); } break;
                case StatResult::OtherError: { /* 500 */ } break;
            }

            return;
        }
    }

    if (file_info.type == FileType::File) {
        ServeFile(filename, file_info, request, io);
    } else if (file_info.type == FileType::Directory) {
        if (!EndsWith(request.url, "/")) {
            const char *redirect = Fmt(&io->allocator, "%1/", request.url).ptr;
            io->AddHeader("Location", redirect);
            io->AttachNothing(302);
            return;
        }

        const char *index_filename = Fmt(&io->allocator, "%1/index.html", filename).ptr;

        FileInfo index_info;

        if (StatFile(index_filename, (int)StatFlag::IgnoreMissing, &index_info) == StatResult::Success &&
                index_info.type == FileType::File) {
            ServeFile(index_filename, index_info, request, io);
        } else if (config.auto_index) {
            ServeIndex(filename, request, io);
        } else {
            LogError("Cannot access directory without index.html");
            io->AttachError(403);
            return;
        }
    } else if (file_info.type != FileType::File) {
        io->AttachError(403);
        return;
    }
}


int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    BlockAllocator temp_alloc;

    // Default config filename
    const char *config_filename = Fmt(&temp_alloc, "%1%/serf.ini", GetApplicationDirectory()).ptr;
    bool explicit_config = false;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, 
R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

    %!..+-R, --root_dir <dir>%!0         Change root directory
                                 %!D..(default: %3)%!0
        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %4)%!0)",
                FelixTarget, config_filename, config.root_directory, config.http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Find config filename
    {
        OptionParser opt(argc, argv, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
                explicit_config = true;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Load config
    if (!explicit_config && !TestFile(config_filename)) {
        config_filename = nullptr;
    }
    if (config_filename && !LoadConfig(config_filename, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--root_dir", OptionType::Value)) {
                config.root_directory = opt.current_value;
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &config.http.port))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!config.Validate())
            return 1;
    }

    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Start(config.http, HandleRequest))
        return 1;

#ifdef __linux__
    if (!NotifySystemd())
        return 1;
#endif

    // Run until exit signal
    {
        bool run = true;

        while (run) {
            WaitForResult ret = WaitForInterrupt();

            if (ret == WaitForResult::Interrupt) {
                LogInfo("Exit requested");
                run = false;
            }
        }
    }

    LogDebug("Stop HTTP server");
    daemon.Stop();

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
