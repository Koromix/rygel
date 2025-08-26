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
#include "src/core/request/s3.hh"

namespace RG {

static bool ConnectToS3(s3_Client *s3, const char *url)
{
    s3_Config config;
    if (!s3_DecodeURL(url, &config))
        return false;
    if (!config.Complete())
        return false;
    if (!config.Validate())
        return false;

    return s3->Open(config);
}

static int RunList(Span<const char *> arguments)
{
    // Options
    const char *url = nullptr;
    const char *prefix = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 list url [prefix])", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        url = opt.ConsumeNonOption();
        prefix = opt.ConsumeNonOption();

        opt.LogUnusedArguments();
    }

    if (!url) {
        LogError("Missing S3 URL");
        return 1;
    }

    s3_Client s3;
    if (!ConnectToS3(&s3, url))
        return 1;

    bool success = s3.ListObjects(prefix, [](const char *path, int64_t size) {
        PrintLn("%!..+%1%!0 %2", FmtArg(path).Pad(34), FmtDiskSize(size));
        return true;
    });
    if (!success)
        return 1;

    return 0;
}

static int RunHead(Span<const char *> arguments)
{
    // Options
    const char *url = nullptr;
    const char *key = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 head url key)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        url = opt.ConsumeNonOption();
        key = opt.ConsumeNonOption();

        opt.LogUnusedArguments();
    }

    if (!url) {
        LogError("Missing S3 URL");
        return 1;
    }
    if (!key) {
        LogError("Missing object key");
        return 1;
    }

    s3_Client s3;
    if (!ConnectToS3(&s3, url))
        return 1;

    s3_ObjectInfo info = {};
    StatResult ret = s3.HeadObject(key, &info);

    switch (ret) {
        case StatResult::Success: {
            PrintLn("Object exists: %!..+%1%!0", FmtDiskSize(info.size));
            if (info.version[0]) {
                PrintLn("Version ID: %!D..%1%!0", info.version);
            }
            return 0;
        } break;
        case StatResult::MissingPath: {
            PrintLn("Object does not exist");
            return 1;
        } break;

        case StatResult::AccessDenied:
        case StatResult::OtherError: return 1;
    }

    return 0;
}

static int RunGet(Span<const char *> arguments)
{
    // Options
    const char *url = nullptr;
    const char *key = nullptr;
    const char *dest_filename = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 get url key destination)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        url = opt.ConsumeNonOption();
        key = opt.ConsumeNonOption();
        dest_filename = opt.ConsumeNonOption();

        opt.LogUnusedArguments();
    }

    if (!url) {
        LogError("Missing S3 URL");
        return 1;
    }
    if (!key) {
        LogError("Missing object key");
        return 1;
    }
    if (!dest_filename) {
        LogError("Missing destination filename");
        return 1;
    }

    s3_Client s3;
    if (!ConnectToS3(&s3, url))
        return 1;

    StreamWriter writer(dest_filename);
    if (!writer.IsValid())
        return 1;

    const auto func = [&](int64_t offset, Span<const uint8_t> buf) {
        if (!offset && !writer.Rewind())
            return false;

        return writer.Write(buf);
    };

    s3_ObjectInfo info = {};
    bool success = s3.GetObject(key, func, &info);

    if (!success)
        return 1;
    if (!writer.Close())
        return 1;

    PrintLn("Size: %!..+%1%!0", FmtDiskSize(info.size));
    if (info.version[0]) {
        PrintLn("Version ID: %!D..%1%!0", info.version);
    }

    return 0;
}

static int RunPut(Span<const char *> arguments)
{
    // Options
    s3_PutSettings settings = {};
    const char *url = nullptr;
    const char *src_filename = nullptr;
    const char *key = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 put [option...] url source key

Options:

    %!..+-t, --mimetype type%!0            Set object mimetype (Content-Type)

        %!..+--conditional%!0              Ask for conditional write (If-None-Match))", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-t", "--mimetype", OptionType::Value)) {
                settings.mimetype = opt.current_value;
            } else if (opt.Test("--conditional")) {
                settings.conditional = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        url = opt.ConsumeNonOption();
        src_filename = opt.ConsumeNonOption();
        key = opt.ConsumeNonOption();

        opt.LogUnusedArguments();
    }

    if (!url) {
        LogError("Missing S3 URL");
        return 1;
    }
    if (!src_filename) {
        LogError("Missing destination filename");
        return 1;
    }
    if (!key) {
        LogError("Missing object key");
        return 1;
    }

    s3_Client s3;
    if (!ConnectToS3(&s3, url))
        return 1;

    StreamReader reader(src_filename);
    if (!reader.IsValid())
        return 1;

    int64_t size = reader.ComputeRawLen();
    if (size < 0) {
        LogError("Cannot send file of unknown length");
        return 1;
    }

    const auto func = [&](int64_t offset, Span<uint8_t> buf) {
        if (!offset && !reader.Rewind())
            return (Size)-1;

        return reader.Read(buf);
    };

    s3_PutResult ret = s3.PutObject(key, size, func, settings);

    switch (ret) {
        case s3_PutResult::Success: return 0;
        case s3_PutResult::ObjectExists: {
            LogError("Object '%1' already exists", key);
            return 1;
        } break;
        case s3_PutResult::OtherError: return 1;
    }

    RG_UNREACHABLE();
}

static int RunDelete(Span<const char *> arguments)
{
    // Options
    const char *url = nullptr;
    const char *key = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 delete [option...] url key)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        url = opt.ConsumeNonOption();
        key = opt.ConsumeNonOption();

        opt.LogUnusedArguments();
    }

    if (!url) {
        LogError("Missing S3 URL");
        return 1;
    }
    if (!key) {
        LogError("Missing object key");
        return 1;
    }

    s3_Client s3;
    if (!ConnectToS3(&s3, url))
        return 1;

    if (!s3.DeleteObject(key))
        return 1;

    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 command [arg...]%!0

Commands:

    %!..+list%!0                           List objects
    %!..+head%!0                           Test object
    %!..+get%!0                            Get object
    %!..+put%!0                            Put object
    %!..+delete%!0                         Delete object

Use %!..+%1 help command%!0 or %!..+%1 command --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(StdErr);
        PrintLn(StdErr);
        LogError("No command provided");
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle help and version arguments
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = (cmd[0] == '-') ? cmd : "--help";
        } else {
            print_usage(StdOut);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
        return 0;
    }

    // Execute relevant command
    if (TestStr(cmd, "list")) {
        return RunList(arguments);
    } else if (TestStr(cmd, "head")) {
        return RunHead(arguments);
    } else if (TestStr(cmd, "get")) {
        return RunGet(arguments);
    } else if (TestStr(cmd, "put")) {
        return RunPut(arguments);
    } else if (TestStr(cmd, "delete")) {
        return RunDelete(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
