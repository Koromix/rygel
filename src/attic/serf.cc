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

    if (file_info.type == FileType::Directory) {
        filename = Fmt(&io->allocator, "%1/index.html", filename).ptr;

        if (StatFile(filename, &file_info) != StatResult::Success) {
            LogError("Cannot access directory without index.html");
            io->AttachError(403);
            return;
        }
    } else if (file_info.type != FileType::File) {
        io->AttachError(403);
        return;
    }

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


int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    BlockAllocator temp_alloc;

    // Default config filename
    LocalArray<const char *, 4> config_filenames;
    const char *config_filename = FindConfigFile("serf.ini", &temp_alloc, &config_filenames);

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
                FelixTarget, FmtSpan(config_filenames.As()), config.root_directory, config.http.port);
    };

    // Find config filename
    {
        OptionParser opt(argc, argv, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
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
