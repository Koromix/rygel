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
#include "src/core/network/network.hh"

namespace RG {

struct HttpHeader {
    const char *key;
    const char *value;
};

struct Config {
    http_Config http { 8000 };

    const char *root_directory = ".";
    bool auto_index = true;
    bool auto_html = true;
    HeapArray<HttpHeader> headers;

    bool set_etag = true;
    int64_t max_age = 0;
    bool verbose = false;

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
    if (!TestFile(config.root_directory, FileType::Directory)) {
        LogError("Root directory '%1' does not exist", config.root_directory);
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
                    } else if (prop.key == "AutoHtml") {
                        valid &= ParseBool(prop.value, &config.auto_html);
                    } else if (prop.key == "MaxAge") {
                        valid &= ParseDuration(prop.value, &config.max_age);
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
            if (config.verbose) {
                LogInfo("Serving file '%1' (cache)", filename);
            }

            MHD_Response *response = MHD_create_response_empty((MHD_ResponseFlags)0);
            io->AttachResponse(304, response);
            return;
        }
    }

    if (config.verbose) {
        LogInfo("Serving file '%1'", filename);
    }

    // Send the file
    const char *mimetype = GetMimeType(GetPathExtension(filename));
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
        <style>
            html { height: 100%; }
            body {
                display: flex;
                width: 1000px;
                max-width: calc(100% - 50px);
                padding: 0;
                margin: 0 auto;
                justify-content: center;
                color: #383838;
                line-height: 1.5;
                flex-direction: column;
            }

            nav {
                padding: 1em;
            }
            main {
                flex: 1;
                margin-bottom: 25px;
                padding: 1em;
                background: #f6f6f6;
            }

            a {
                text-decoration: none;
                font-weight: normal;
                color: #24579d;
            }
            a:hover { text-decoration: underline; }

            ul {
                padding-left: 1em;
                color: #24579d;
            }
            li > a { color: inherit; }
            li.directory {
                color: #383838;
                list-style-type: disc;
            }
            li.file { list-style-type: circle; }
        </style>
    </head>
    <body>
        <nav>
{{ NAV }}
        </nav>
        <main>
{{ MAIN }}
        </main>
    </body>
</html>
)";

    if (config.verbose) {
        LogInfo("Serving index for '%1'", dirname);
    }

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
                Span<const char> stripped = TrimStrRight(request.url, "/");
                Span<const char> title = Fmt(&io->allocator, "%1/", SplitStrReverseAny(stripped, RG_PATH_SEPARATORS));

                WriteContent(title, writer);
            } else if (key == "NAV") {
                bool root = TestStr(request.url, "/");

                PrintLn(writer, "        <a href=\"..\"%1>(go back)</a>", root ? " style=\"visibility: hidden;\"" : "");
                PrintLn(writer, "        %1", request.url);
            } else if (key == "MAIN") {
                if (names.len) {
                    writer->Write("        <ul>\n");
                    for (Span<const char> name: names) {
                        const char *cls = EndsWith(name, "/") ? "directory" : "file";

                        Print(writer, "            <li class=\"%1\"><a href=\"", cls); WriteURL(name, writer); writer->Write("\">");
                        WriteContent(name, writer); writer->Write("</a></li>\n");
                    }
                    writer->Write("        </ul>");
                } else {
                    writer->Write("Empty directory");
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

    Span<const char> relative_url = TrimStrLeft(request.url, "/\\").ptr;
    Span<const char> filename = NormalizePath(relative_url, config.root_directory, &io->allocator);

    FileInfo file_info;
    {
        StatResult stat = StatFile(filename.ptr, (int)StatFlag::IgnoreMissing, &file_info);

        if (config.auto_html) {
            if (stat == StatResult::MissingPath && !EndsWith(filename, "/")
                                                && !GetPathExtension(filename).len) {
                filename = Fmt(&io->allocator, "%1.html", filename).ptr;
                stat = StatFile(filename.ptr, (int)StatFlag::IgnoreMissing, &file_info);
            }
        }

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
        ServeFile(filename.ptr, file_info, request, io);
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
            ServeIndex(filename.ptr, request, io);
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

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st, 
R"(Usage: %!..+%1 [options] [root]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

    %!..+-p, --port <port>%!0            Change web server port
                                 %!D..(default: %4)%!0

    %!..+-v, --verbose%!0                Log served requests)",
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
                print_usage(StdOut);
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
            } else if (opt.Test("-p", "--port", OptionType::Value)) {
                if (!config.http.SetPortOrPath(opt.current_value))
                    return 1;
            } else if (opt.Test("-v", "--verbose")) {
                config.verbose = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *root_directory = opt.ConsumeNonOption();
        config.root_directory = root_directory ? root_directory : config.root_directory;

        opt.LogUnusedArguments();

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
