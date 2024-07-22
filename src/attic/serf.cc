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
#include "src/core/http/http.hh"
#include "src/core/request/curl.hh"

namespace RG {

struct Config {
    http_Config http { 8000 };

    const char *root_directory = nullptr;
    bool auto_index = true;
    bool explicit_index = false;
    bool auto_html = true;

    const char *proxy_url = nullptr;
    bool proxy_first = false;
    int connect_timeout = 5000;
    int connect_retries = 3;
    int max_time = 60000;

    HeapArray<http_KeyValue> headers;

    bool set_etag = true;
    int64_t max_age = 0;
    bool verbose = false;

    BlockAllocator str_alloc;

    bool Validate();
};

static Config config;

static thread_local CURL *curl = nullptr;

static const char *NormalizeURL(const char *url, Allocator *alloc)
{
    CURLU *h = curl_url();
    RG_DEFER { curl_url_cleanup(h); };

    // Parse URL
    {
        CURLUcode ret = curl_url_set(h, CURLUPART_URL, url, CURLU_NON_SUPPORT_SCHEME);
        if (ret == CURLUE_OUT_OF_MEMORY)
            RG_BAD_ALLOC();
        if (ret != CURLUE_OK) {
            LogError("Malformed URL '%1'", url);
            return nullptr;
        }
    }

    const char *scheme = curl_GetUrlPartStr(h, CURLUPART_SCHEME, alloc).ptr;
    const char *normalized = curl_GetUrlPartStr(h, CURLUPART_URL, alloc).ptr;

    if (scheme && !TestStr(scheme, "http") && !TestStr(scheme, "https")) {
        LogError("Unsupported proxy scheme '%1'", scheme);
        return nullptr;
    }
    if (!EndsWith(normalized, "/")) {
        LogError("Proxy URL '%1' should end with '/'", normalized);
        return nullptr;
    }

    return normalized;
}

bool Config::Validate()
{
    bool valid = true;

    valid &= http.Validate();
    if (max_age < 0) {
        LogError("HTTP MaxAge must be >= 0");
        valid = false;
    }
    if (!root_directory && !proxy_url) {
        LogError("Neither file nor reverse proxy is configured");
        valid = false;
    }
    if (root_directory && !TestFile(root_directory, FileType::Directory)) {
        LogError("Root directory '%1' does not exist", root_directory);
        valid = false;
    }
    if (proxy_url) {
        if (auto_index) {
            if (explicit_index) {
                LogError("AutoIndex is not allowed when a reverse proxy is configured");
                valid = false;
            } else {
                auto_index = false;
            }
        }

        const char *url = NormalizeURL(proxy_url, &str_alloc);

        proxy_url = url ? url : proxy_url;
        valid &= !!url;
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
                        if (ParseBool(prop.value, &config.auto_index)) {
                            config.explicit_index = true;
                        } else {
                            valid = false;
                        }
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
            } else if (prop.section == "Proxy") {
                do {
                    if (prop.key == "RemoteUrl") {
                        config.proxy_url = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "ProxyFirst") {
                        valid &= ParseBool(prop.value, &config.proxy_first);
                    } else if (prop.key == "ConnectTimeout") {
                        valid &= ParseDuration(prop.value, &config.connect_timeout);
                    } else if (prop.key == "RetryCount") {
                        if (ParseInt(prop.value, &config.connect_retries)) {
                            if (config.connect_retries < 0) {
                                LogError("Invalid RetryCount value");
                                valid = false;
                            }
                        }
                    } else if (prop.key == "MaxTime") {
                        valid &= ParseDuration(prop.value, &config.max_time);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Headers") {
                do {
                    http_KeyValue header = {};

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
    const char *etag = config.set_etag ? Fmt(io->Allocator(), "%1-%2", file_info.mtime, file_info.size).ptr : nullptr;

    // Handle ETag caching
    if (etag) {
        const char *client_etag = request.FindHeader("If-None-Match");

        if (client_etag && TestStr(client_etag, etag)) {
            if (config.verbose) {
                LogInfo("Serving '%1' with 304 (valid cache ETag)", request.path, filename);
            }

            io->SendEmpty(304);
            return;
        }
    }

    if (config.verbose) {
        LogInfo("Serving '%1' with '%2'", request.path, filename);
    }

    const char *mimetype = GetMimeType(GetPathExtension(filename));
    io->AddCachingHeaders(config.max_age, etag);
    if (mimetype) {
        io->AddHeader("Content-Type", mimetype);
    }

    // Send the file
    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return;
    io->SendFile(200, fd, file_info.size);
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
    if (config.verbose) {
        LogInfo("Serving '%1' with auto-index of '%2'", request.path, dirname);
    }

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

    HeapArray<Span<const char>> names;
    {
        EnumResult ret = EnumerateDirectory(dirname, nullptr, 4096,
                                            [&](const char *basename, FileType file_type) {
            Span<const char> filename = Fmt(io->Allocator(), "%1%2", basename, file_type == FileType::Directory ? "/" : "");
            names.Append(filename);

            return true;
        });

        if (ret != EnumResult::Success) {
            switch (ret) {
                case EnumResult::Success: { RG_UNREACHABLE(); } break;

                case EnumResult::MissingPath: { io->SendError(404); } break;
                case EnumResult::AccessDenied: { io->SendError(403); } break;
                case EnumResult::PartialEnum: {
                    LogError("Too many files");
                    io->SendError(413);
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

    Span<const uint8_t> page = PatchFile(IndexTemplate.As<const uint8_t>(), io->Allocator(),
                                         [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            Span<const char> stripped = TrimStrRight(request.path, "/");
            Span<const char> title = Fmt(io->Allocator(), "%1/", SplitStrReverseAny(stripped, RG_PATH_SEPARATORS));

            WriteContent(title, writer);
        } else if (key == "NAV") {
            bool root = TestStr(request.path, "/");

            PrintLn(writer, "        <a href=\"..\"%1>(go back)</a>", root ? " style=\"visibility: hidden;\"" : "");
            PrintLn(writer, "        %1", request.path);
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

    io->SendBinary(200, page, "text/html");
}

static bool HandleLocal(const http_RequestInfo &request, http_IO *io)
{
    if (!config.root_directory)
        return false;

    Span<const char> relative_url = TrimStrLeft(request.path, "/\\").ptr;
    Span<const char> filename = NormalizePath(relative_url, config.root_directory, io->Allocator());

    FileInfo file_info;
    {
        StatResult stat = StatFile(filename.ptr, (int)StatFlag::SilentMissing, &file_info);

        if (config.auto_html) {
            if (stat == StatResult::MissingPath && !EndsWith(filename, "/")
                                                && !GetPathExtension(filename).len) {
                filename = Fmt(io->Allocator(), "%1.html", filename).ptr;
                stat = StatFile(filename.ptr, (int)StatFlag::SilentMissing, &file_info);
            }
        }

        switch (stat) {
            case StatResult::Success: {} break;

            case StatResult::MissingPath: return false;
            case StatResult::AccessDenied: {
                io->SendError(403);
                return true;
            } break;
            case StatResult::OtherError: return true;
        }
    }

    if (file_info.type == FileType::File) {
        ServeFile(filename.ptr, file_info, request, io);
        return true;
    } else if (file_info.type == FileType::Directory) {
        if (!EndsWith(request.path, "/")) {
            const char *redirect = Fmt(io->Allocator(), "%1/", request.path).ptr;
            io->AddHeader("Location", redirect);
            io->SendEmpty(302);
            return true;
        }

        const char *index_filename = Fmt(io->Allocator(), "%1/index.html", filename).ptr;

        FileInfo index_info;

        if (StatFile(index_filename, (int)StatFlag::SilentMissing, &index_info) == StatResult::Success &&
                index_info.type == FileType::File) {
            ServeFile(index_filename, index_info, request, io);
            return true;
        } else if (config.auto_index) {
            ServeIndex(filename.ptr, request, io);
            return true;
        } else {
            return false;
        }
    } else {
        io->SendError(403);
        return true;
    }
}

static bool HandleProxy(const http_RequestInfo &request, http_IO *io)
{
    if (!config.proxy_url)
        return false;

    static const char *const OmitHeaders[] = {
        "Host",
        "Referer",
        "Sec-*",
        "server",
        "Connection",
        "Keep-Alive",
        "Content-Length",
        "Transfer-Encoding"
    };

    if (curl) {
        if (!curl_Reset(curl))
            return false;
    } else {
        curl = curl_Init();
        if (!curl)
            return false;
        atexit([] { curl_easy_cleanup(curl); });
    }

    const char *relative_url = TrimStrLeft(request.path, '/').ptr;
    const char *url = Fmt(io->Allocator(), "%1%2", config.proxy_url, relative_url).ptr;
    HeapArray<curl_slist> curl_headers;

    // Copy client headers
    for (const http_KeyValue &header: request.headers) {
        bool skip = std::any_of(std::begin(OmitHeaders), std::end(OmitHeaders),
                                [&](const char *pattern) { return MatchPathName(header.key, pattern, false); });

        if (!skip) {
            char *str = Fmt(io->Allocator(), "%1: %2", header.key, header.value).ptr;
            curl_headers.Append({ .data = str, .next = nullptr });
        }
    }
    for (Size i = 0; i < curl_headers.len - 1; i++) {
        curl_headers[i].next = &curl_headers[i + 1];
    }

    struct RelayContext {
        http_IO *io;

        HeapArray<http_KeyValue> headers;
        HeapArray<uint8_t> data;
    };
    RelayContext ctx;
    ctx.io = io;
    ctx.data.allocator = io->Allocator();

    // Set CURL options
    {
        bool success = true;

        success &= !curl_easy_setopt(curl, CURLOPT_URL, url);
        success &= !curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)config.connect_timeout);
        success &= !curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)config.max_time);
        success &= !curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers.ptr);

        success &= !curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                                     +[](char *ptr, size_t, size_t nmemb, void *udata) {
            RelayContext *ctx = (RelayContext *)udata;
            http_IO *io = ctx->io;

            char *separator = (char *)memchr(ptr, ':', nmemb);
            if (!separator)
                return nmemb;
            char *end = ptr + nmemb;

            Span<char> key = MakeSpan(ptr, separator - ptr);
            Span<char> value = TrimStr(MakeSpan(separator + 1, end - separator - 1));

            // Needed to use MatchPathName
            key.ptr[key.len] = 0;

            bool skip = std::any_of(std::begin(OmitHeaders), std::end(OmitHeaders),
                                    [&](const char *pattern) { return MatchPathName(key.ptr, pattern, false); });

            if (!skip) {
                key = DuplicateString(key, io->Allocator());
                value = DuplicateString(value, io->Allocator());

                ctx->headers.Append({ key.ptr, value.ptr });
            }

            return nmemb;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ctx);

        success &= !curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                     +[](char *ptr, size_t, size_t nmemb, void *udata) {
            RelayContext *ctx = (RelayContext *)udata;

            Span<const uint8_t> buf = MakeSpan((const uint8_t *)ptr, (Size)nmemb);
            ctx->data.Append(buf);

            return nmemb;
        });
        success &= !curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

        if (!success) {
            LogError("Failed to set libcurl options");
            return true;
        }
    }

    int status = 0;
    for (int i = 0; i <= config.connect_retries; i++) {
        ctx.headers.Clear();
        ctx.data.Clear();

        if (i) {
            int delay = 200 + 100 * (1 << i);
            delay += !!i * GetRandomInt(0, delay / 2);

            WaitDelay(delay);
        }

        int64_t start = GetMonotonicTime();

        status = curl_Perform(curl, "HTTP");

        if (status == -CURLE_COULDNT_RESOLVE_PROXY ||
                status == -CURLE_COULDNT_RESOLVE_HOST ||
                status == -CURLE_COULDNT_CONNECT ||
                status == -CURLE_SSL_CONNECT_ERROR)
            continue;
        if (status == -CURLE_OPERATION_TIMEDOUT &&
                GetMonotonicTime() - start < config.max_time)
            continue;

        break;
    }

    if (status == 404)
        return false;

    if (config.verbose) {
        LogInfo("Proxying '%1' from '%2'", request.path, url);
    }

    if (status < 0) {
        io->SendError(502);
        return true;
    }

    for (const http_KeyValue &header: ctx.headers) {
        io->AddHeader(header.key, header.value);
    }
    io->SendBinary(status, ctx.data.Leak());

    return true;
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
    RG_ASSERT(StartsWith(request.path, "/"));

    // Security checks
    if (request.method != http_RequestMethod::Get) {
        LogError("Only GET requests are allowed");
        io->SendError(405);
        return;
    }
    if (PathContainsDotDot(request.path)) {
        LogError("Unsafe URL containing '..' components");
        io->SendError(403);
        return;
    }

    // Add configured headers
    for (const http_KeyValue &header: config.headers) {
        io->AddHeader(header.key, header.value);
    }

    if (config.proxy_first && HandleProxy(request, io))
        return;
    if (HandleLocal(request, io))
        return;
    if (!config.proxy_first && HandleProxy(request, io))
        return;

    LogInfo("Cannot find anything to serve '%1'", request.path);
    io->SendError(404);
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
                                 %!D..(default: %3)%!0

        %!..+--proxy <url>%!0            Reverse proxy unknown URLs to this server
        %!..+--proxy_first%!0            Prefer proxy URLs to local files

        %!..+--enable_sab%!0             Set headers for SharedArrayBuffer support

    %!..+-v, --verbose%!0                Log served requests)",
                FelixTarget, config_filename, config.http.port);
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

    if (curl_global_init(CURL_GLOBAL_ALL)) {
        LogError("Failed to initialize libcurl");
        return 1;
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
            } else if (opt.Test("--proxy", OptionType::Value)) {
                config.proxy_url = opt.current_value;
            } else if (opt.Test("--proxy_first")) {
                config.proxy_first = true;
            } else if (opt.Test("--enable_sab")) {
                Size j = 0;
                for (Size i = 0; i < config.headers.len; i++) {
                    const http_KeyValue &header = config.headers[i];
                    config.headers[j] = header;

                    bool keep = !TestStrI(header.key, "Cross-Origin-Opener-Policy") &&
                                !TestStrI(header.key, "Cross-Origin-Embedder-Policy");
                    j += keep;
                }
                config.headers.len = j;

                config.headers.Append({ "Cross-Origin-Opener-Policy", "same-origin" });
                config.headers.Append({ "Cross-Origin-Embedder-Policy", "require-corp" });
            } else if (opt.Test("-v", "--verbose")) {
                config.verbose = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *root_directory = opt.ConsumeNonOption();
        config.root_directory = root_directory ? root_directory : config.root_directory;

        if (!config_filename && !config.root_directory && !config.proxy_url) {
            config.root_directory = ".";
        }

        opt.LogUnusedArguments();

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!config.Validate())
            return 1;
    }

    LogInfo("Init HTTP server");

    http_Daemon daemon;
    if (!daemon.Bind(config.http))
        return 1;
    if (!daemon.Start(HandleRequest))
        return 1;

#if defined(__linux__)
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
