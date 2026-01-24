// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"
#include "lib/native/request/curl.hh"

namespace K {

enum class SourceType {
    Local,
    Remote
};

struct SourceInfo {
    SourceType type;
    const char *path;
};

struct FilterInfo {
    const char *extension;
    const char *command;

    K_HASHTABLE_HANDLER(FilterInfo, extension);
};

struct ServiceInfo {
    const char *name;
    const char *command;
};

struct Config {
    http_Config http { 8000 };

    HeapArray<SourceInfo> sources;

    bool auto_index = true;
    bool explicit_index = false;
    bool auto_html = true;
    bool follow_symlinks = false;
    int connect_timeout = 5000;
    int connect_retries = 3;
    int max_time = 60000;

    HeapArray<http_KeyValue> headers;

    bool set_etag = true;
    int64_t max_age = 0;
    bool verbose = false;

    HashMap<const char *, const char *> mimetypes;
    HashTable<const char *, FilterInfo> filters;
    HeapArray<ServiceInfo> services;

    BlockAllocator str_alloc;

    void AppendSource(const char *path, Span<const char> root_directory);

    bool Validate(bool require_sources = true);
};

enum class HandlerResult {
    Done,
    Missing,
    Error
};

static Config config;

static thread_local CURL *curl = nullptr;

static const char *NormalizeURL(const char *url, Allocator *alloc)
{
    CURLU *h = curl_url();
    K_DEFER { curl_url_cleanup(h); };

    // Parse URL
    {
        CURLUcode ret = curl_url_set(h, CURLUPART_URL, url, CURLU_NON_SUPPORT_SCHEME);
        if (ret == CURLUE_OUT_OF_MEMORY)
            K_BAD_ALLOC();
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

static bool LooksLikeURL(const char *path)
{
    while (IsAsciiAlphaOrDigit(path[0])) {
        path++;
    }
    return StartsWith(path, "://");
}

static bool IsAllowed(FileType type)
{
    if (type == FileType::Directory)
        return true;
    if (type == FileType::File)
        return true;
    if (config.follow_symlinks && type == FileType::Link)
        return true;

    return false;
}

static bool IsDirectory(FileType type)
{
    return type == FileType::Directory;
}

void Config::AppendSource(const char *path, Span<const char> root_directory)
{
    SourceInfo src = {};

    if (LooksLikeURL(path)) {
        src.type = SourceType::Remote;
        src.path = DuplicateString(path, &str_alloc).ptr;
    } else {
        src.type = SourceType::Local;
        src.path = NormalizePath(path, root_directory, &str_alloc).ptr;
    }

    sources.Append(src);
}

bool Config::Validate(bool require_sources)
{
    bool valid = true;

    valid &= http.Validate();
    if (max_age < 0) {
        LogError("HTTP MaxAge must be >= 0");
        valid = false;
    }

    if (require_sources && !sources.len) {
        LogError("No source is configured");
        valid = false;
    }
    for (SourceInfo &src: sources) {
        switch (src.type) {
            case SourceType::Local: {
                if (!TestFile(src.path, FileType::Directory)) {
                    LogError("Directory '%1' does not exist", src.path);
                    valid = false;
                }
            } break;

            case SourceType::Remote: {
                const char *normalized = NormalizeURL(src.path, &str_alloc);

                src.path = normalized ? normalized : src.path;
                valid &= !!normalized;
            } break;
        }
    }

    if (auto_index) {
        if (sources.len > 1) {
            if (explicit_index) {
                LogError("AutoIndex is not allowed when multiple sources are configured");
                valid = false;
            } else {
                auto_index = false;
            }
        } else if (sources.len == 1 && sources[0].type != SourceType::Local) {
            if (explicit_index) {
                LogError("AutoIndex is not allowed when a non-local source is used");
                valid = false;
            } else {
                auto_index = false;
            }
        }
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
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "HTTP") {
                valid &= config.http.SetProperty(prop.key.ptr, prop.value.ptr, root_directory);
            } else if (prop.section == "Settings") {
                if (prop.key == "AutoIndex") {
                    if (ParseBool(prop.value, &config.auto_index)) {
                        config.explicit_index = true;
                    } else {
                        valid = false;
                    }
                } else if (prop.key == "AutoHtml") {
                    valid &= ParseBool(prop.value, &config.auto_html);
                } else if (prop.key == "FollowSymlinks") {
                    valid &= ParseBool(prop.value, &config.follow_symlinks);
                } else if (prop.key == "MaxAge") {
                    valid &= ParseDuration(prop.value, &config.max_age);
                } else if (prop.key == "ETag") {
                    valid &= ParseBool(prop.value, &config.set_etag);
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
            } else if (prop.section == "Sources") {
                if (prop.key == "Source") {
                    config.AppendSource(prop.value.ptr, root_directory);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Headers") {
                http_KeyValue header = {};

                header.key = DuplicateString(prop.key, &config.str_alloc).ptr;
                header.value = DuplicateString(prop.value, &config.str_alloc).ptr;

                config.headers.Append(header);
            } else if (prop.section == "Mimetypes") {
                const char *extension = DuplicateString(prop.key, &config.str_alloc).ptr;
                const char *mimetype = DuplicateString(prop.value, &config.str_alloc).ptr;

                config.mimetypes.Set(extension, mimetype);
            } else if (prop.section == "Filters") {
                FilterInfo filter = {};

                filter.extension = DuplicateString(prop.key, &config.str_alloc).ptr;
                filter.command = DuplicateString(prop.value, &config.str_alloc).ptr;

                config.filters.Set(filter);
            } else if (prop.section == "Services") {
                ServiceInfo service = {};

                service.name = DuplicateString(prop.key, &config.str_alloc).ptr;
                service.command = DuplicateString(prop.value, &config.str_alloc).ptr;

                config.services.Append(service);
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
    if (!config.Validate(false))
        return false;

    std::swap(*out_config, config);
    return true;
}

static bool LoadConfig(const char *filename, Config *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

static void ServeFile(http_IO *io, const char *filename, const FileInfo &file_info)
{
    const http_RequestInfo &request = io->Request();
    const char *etag = config.set_etag ? Fmt(io->Allocator(), "%1-%2", file_info.mtime, file_info.size).ptr : nullptr;

    // Handle ETag caching
    if (etag) {
        const char *client_etag = request.GetHeaderValue("If-None-Match");

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

    Span<const char> extension = GetPathExtension(filename);
    const char *mimetype = config.mimetypes.FindValue(extension, nullptr);
    const FilterInfo *filter = config.filters.Find(extension);

    if (!mimetype) {
        mimetype = GetMimeType(extension);
    }

    if (mimetype) {
        io->AddHeader("Content-Type", mimetype);
    }
    io->AddCachingHeaders(config.max_age, etag);

    // Send file directly or transformed (by handler command)
    if (filter) {
        StreamReader reader(filename);
        if (!reader.IsValid())
            return;

        StreamWriter writer;
        if (!io->OpenForWrite(200, -1, &writer))
            return;

        uint8_t buf[16384];
        auto read = [&]() {
            Size len = reader.Read(buf);
            return MakeSpan(buf, std::max(len, (Size)0));
        };
        auto write = [&](const Span<uint8_t> buf) { writer.Write(buf); };

        ExecuteInfo info = {};

        info.work_dir = DuplicateString(GetPathDirectory(filename), io->Allocator()).ptr;

        int code;
        bool success = ExecuteCommandLine(filter->command, info, read, write, &code);

        if (success && code != 0) {
            // Can't do much more and inform client properly, response status has been sent already
            LogError("Handler command for '%1' failed with code %2", filename, code);
        }
    } else {
        int fd = OpenFile(filename, (int)OpenFlag::Read);
        if (fd < 0)
            return;
        io->SendFile(200, fd, file_info.size);
    }
}

static void WriteContent(Span<const char> str, StreamWriter *writer) {
    for (char c: str) {
        if (c == '&') {
            writer->Write("&amp;");
        } else if (c == '<') {
            writer->Write("&lt;");
        } else if (c == '>') {
            writer->Write("&gt;");
        } else if (IsAsciiControl(c)) {
            Print(writer, "<0x%1>", FmtHex((uint8_t)c, 2));
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
            Print(writer, "%%%1", FmtHex((uint8_t)c, 2));
        }
    }
}

static void ServeIndex(http_IO *io, const char *dirname)
{
    const http_RequestInfo &request = io->Request();

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

            table {
                width: 100%;
                table-layout: fixed;
            }
            table td:last-child {
                width: 100px;
                text-align: right;
            }
            tr.directory a { color: #383838; }
            tr.other { text-decoration: line-through; }
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
</html>)";

    struct EntryData {
        Span<const char> name;
        FileType type;
        int64_t size;
    };

    HeapArray<EntryData> entries;
    {
        EnumResult ret = EnumerateDirectory(dirname, nullptr, 16384,
                                            [&](const char *basename, const FileInfo &file_info) {
            EntryData entry = {};

            entry.name = Fmt(io->Allocator(), "%1%2", basename, IsDirectory(file_info.type) ? "/" : "");
            entry.type = file_info.type;
            entry.size = file_info.size;

            entries.Append(entry);

            return true;
        });

        if (ret != EnumResult::Success) {
            switch (ret) {
                case EnumResult::Success: { K_UNREACHABLE(); } break;

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

    std::sort(entries.begin(), entries.end(), [](const EntryData &entry1, const EntryData &entry2) {
        if (IsDirectory(entry1.type) != IsDirectory(entry2.type)) {
            return IsDirectory(entry1.type);
        } else {
            return CmpStr(entry1.name, entry2.name) < 0;
        }
    });

    Span<const uint8_t> page = PatchFile(IndexTemplate.As<const uint8_t>(), io->Allocator(),
                                         [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            Span<const char> stripped = TrimStrRight(request.path, "/");
            Span<const char> title = Fmt(io->Allocator(), "%1/", SplitStrReverseAny(stripped, K_PATH_SEPARATORS));

            WriteContent(title, writer);
        } else if (key == "NAV") {
            bool root = TestStr(request.path, "/");
            Print(writer, "<a href=\"..\"%1>(go back)</a> %2", root ? " style=\"visibility: hidden;\"" : "", request.path);
        } else if (key == "MAIN") {
            if (entries.len) {
                writer->Write("<table>");
                for (const EntryData &entry: entries) {
                    if (IsAllowed(entry.type)) {
                        const char *cls = IsDirectory(entry.type) ? "directory" : "file";

                        Print(writer, "<tr class=\"%1\">", cls);
                        Print(writer, "<td><a href=\"");
                        WriteURL(entry.name, writer); writer->Write("\">"); WriteContent(entry.name, writer);
                        Print(writer, "</a></td>");
                        switch (entry.type) {
                            case FileType::Link: { Print(writer, "<td>[L]</td>"); } break;
                            case FileType::File: { Print(writer, "<td>%1</td>", FmtDiskSize(entry.size)); } break;
                            default: { Print(writer, "<td></td>"); } break;
                        }
                        Print(writer, "</tr>");
                    } else {
                        Print(writer, "<tr class=\"other\"><td>");
                        WriteContent(entry.name, writer);
                        Print(writer, "</td><td></td></tr>");
                    }
                }
                writer->Write("</table>");
            } else {
                writer->Write("Empty directory");
            }
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    io->SendBinary(200, page, "text/html");
}

static HandlerResult HandleLocal(http_IO *io, const char *dirname)
{
    const http_RequestInfo &request = io->Request();

    Span<const char> relative_url = TrimStrLeft(request.path, "/\\").ptr;
    Span<const char> filename = NormalizePath(relative_url, dirname, io->Allocator());

    int stat_flags = (int)StatFlag::SilentMissing |
                     (config.follow_symlinks ? (int)StatFlag::FollowSymlink : 0);

    FileInfo file_info;
    {
        StatResult stat = StatFile(filename.ptr, stat_flags, &file_info);

        if (config.auto_html) {
            if (stat == StatResult::MissingPath && !EndsWith(filename, "/")
                                                && !GetPathExtension(filename).len) {
                filename = Fmt(io->Allocator(), "%1.html", filename).ptr;
                stat = StatFile(filename.ptr, stat_flags, &file_info);
            }
        }

        switch (stat) {
            case StatResult::Success: {} break;

            case StatResult::MissingPath: return HandlerResult::Missing;
            case StatResult::AccessDenied: {
                io->SendError(403);
                return HandlerResult::Done;
            } break;
            case StatResult::OtherError: return HandlerResult::Error;
        }
    }

    if (file_info.type == FileType::File) {
        ServeFile(io, filename.ptr, file_info);
        return HandlerResult::Done;
    } else if (file_info.type == FileType::Directory) {
        if (!EndsWith(request.path, "/")) {
            const char *redirect = Fmt(io->Allocator(), "%1/", request.path).ptr;
            io->AddHeader("Location", redirect);
            io->SendEmpty(302);
            return HandlerResult::Done;
        }

        const char *index_filename = Fmt(io->Allocator(), "%1/index.html", filename).ptr;

        FileInfo index_info;

        if (StatFile(index_filename, stat_flags, &index_info) == StatResult::Success &&
                index_info.type == FileType::File) {
            ServeFile(io, index_filename, index_info);
            return HandlerResult::Done;
        } else if (config.auto_index) {
            ServeIndex(io, filename.ptr);
            return HandlerResult::Done;
        } else {
            io->SendError(403);
            return HandlerResult::Done;
        }
    } else {
        io->SendError(403);
        return HandlerResult::Done;
    }
}

static HandlerResult HandleProxy(http_IO *io, const char *proxy_url, bool relay404)
{
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

    const http_RequestInfo &request = io->Request();

    if (curl) {
        if (!curl_Reset(curl))
            return HandlerResult::Error;
    } else {
        curl = curl_Init();
        if (!curl)
            return HandlerResult::Error;
        atexit([] { curl_easy_cleanup(curl); });
    }

    const char *relative_url = TrimStrLeft(request.path, '/').ptr;
    const char *url = Fmt(io->Allocator(), "%1%2", proxy_url, relative_url).ptr;
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

                ctx->headers.Append({ key.ptr, value.ptr, nullptr });
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
            return HandlerResult::Error;
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

        int64_t start = GetMonotonicClock();

        status = curl_Perform(curl, "HTTP");

        if (status == -CURLE_COULDNT_RESOLVE_PROXY ||
                status == -CURLE_COULDNT_RESOLVE_HOST ||
                status == -CURLE_COULDNT_CONNECT ||
                status == -CURLE_SSL_CONNECT_ERROR)
            continue;
        if (status == -CURLE_OPERATION_TIMEDOUT &&
                GetMonotonicClock() - start < config.max_time)
            continue;

        break;
    }

    if (status == 404 && !relay404)
        return HandlerResult::Missing;

    if (config.verbose) {
        LogInfo("Proxying '%1' from '%2'", request.path, url);
    }

    if (status < 0) {
        io->SendError(502);
        return HandlerResult::Done;
    }

    for (const http_KeyValue &header: ctx.headers) {
        io->AddHeader(header.key, header.value);
    }
    io->SendBinary(status, ctx.data.Leak());

    return HandlerResult::Done;
}

static void HandleRequest(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    K_ASSERT(StartsWith(request.path, "/"));

    // Security checks
    if (request.method != http_RequestMethod::Get) {
        LogError("Only GET requests are allowed");
        io->SendError(405);
        return;
    }

    // Add configured headers
    for (const http_KeyValue &header: config.headers) {
        io->AddHeader(header.key, header.value);
    }

#define TRY(Call) \
        do { \
            HandlerResult ret = (Call); \
             \
            switch (ret) { \
                case HandlerResult::Done: return; \
                case HandlerResult::Missing: break; \
                case HandlerResult::Error: { \
                    io->SendError(500); \
                    return; \
                } break; \
            } \
        } while (false)

    for (const SourceInfo &src: config.sources) {
        switch (src.type) {
            case SourceType::Local: { TRY(HandleLocal(io, src.path)); } break;
            case SourceType::Remote: { TRY(HandleProxy(io, src.path, config.sources.len == 1)); } break;
        }
    }

#undef TRY

    if (config.sources.len > 1) {
        LogError("Cannot find any source for '%1'", request.path);
    }
    io->SendError(404);
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Default config filename
    const char *config_filename = Fmt(&temp_alloc, "%1%/nestor.ini", GetApplicationDirectory()).ptr;
    bool explicit_config = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...] path_or_URL...%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--bind IP%!0                  Bind to specific IP

    %!..+-L, --follow%!0                   Follow symbolic links

        %!..+--sab%!0                      Set headers for SharedArrayBuffer support

    %!..+-v, --verbose%!0                  Log served requests)",
                FelixTarget, config_filename, config.http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
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
            } else if (opt.Test("--bind", OptionType::Value)) {
                config.http.bind_addr = opt.current_value;
            } else if (opt.Test("-L", "--follow")) {
                config.follow_symlinks = true;
            } else if (opt.Test("--sab")) {
                Size j = 0;
                for (Size i = 0; i < config.headers.len; i++) {
                    const http_KeyValue &header = config.headers[i];
                    config.headers[j] = header;

                    bool keep = !TestStrI(header.key, "Cross-Origin-Opener-Policy") &&
                                !TestStrI(header.key, "Cross-Origin-Embedder-Policy");
                    j += keep;
                }
                config.headers.len = j;

                config.headers.Append({ "Cross-Origin-Opener-Policy", "same-origin", nullptr });
                config.headers.Append({ "Cross-Origin-Embedder-Policy", "require-corp", nullptr });
            } else if (opt.Test("-v", "--verbose")) {
                config.verbose = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *arg = opt.ConsumeNonOption();

        if (arg) {
            do {
                config.AppendSource(arg, ".");
            } while ((arg = opt.ConsumeNonOption()));
        } else if (!config.sources.len) {
            config.sources.Append({ SourceType::Local, "." });
        }

        // We may have changed some stuff (such as HTTP port), so revalidate
        if (!config.Validate())
            return 1;
    }

    Async async(1 + config.services.len);

    if (config.services.len) {
        LogInfo("Start services");

        for (Size i = 0; i < config.services.len; i++) {
            async.Run([i]() {
                const ServiceInfo &service = config.services[i];

                Span<const uint8_t> in = {};

                // This won't perfectly split log lines across buffer boundaries, but it's close enough
                const auto out = [&](Span<uint8_t> buf) {
                    char ctx[128];
                    Fmt(ctx, "%1: ", service.name);

                    Span<const char> str = buf.As<const char>();

                    while (str.len) {
                        Span<const char> line = SplitStrLine(str, &str);
                        Log(LogLevel::Info, ctx, "%1", line);
                    }
                };

                // We don't really care about whether it works or not... all that matters it that it keeps running!
                int code;
                ExecuteCommandLine(service.command, {}, in, out, &code);

                PostWaitMessage();
                return false;
            });
        }
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

    // From here on, don't quit abruptly
    WaitEvents(0);

    // Run until exit signal
    int status = 0;
    for (;;) {
        if (!async.IsSuccess()) {
            LogError("Some services have failed");
            status = 1;
            break;
        }

        int timeout = config.services.len ? 60000 : -1;
        WaitResult ret = WaitEvents(timeout);

        if (ret == WaitResult::Exit) {
            LogInfo("Exit requested");
            break;
        } else if (ret == WaitResult::Interrupt) {
            LogInfo("Process interrupted");
            status = 1;
            break;
        }
    }

    LogInfo("Stop HTTP server");
    daemon.Stop();

    if (config.services.len) {
        LogInfo("Stop services");

        PostTerminate();
        async.Sync();
    }

    return status;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
