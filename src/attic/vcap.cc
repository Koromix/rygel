// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"

namespace K {

extern "C" const AssetInfo VcapHtml;

// Configuration
static http_Config http { 8894 };
static const char *dest_directory = ".";

static void HandleRequest(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

#if defined(FELIX_HOT_ASSETS)
    // This is not actually thread safe, because it may release memory from an asset
    // that is being used by another thread. This code only runs in development builds
    // and it pretty much never goes wrong so it is kind of OK.
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        ReloadAssets();
        LogInfo("Reload assets");
    }
#endif

    if (TestStr(request.path, "/") && request.method == http_RequestMethod::Get) {
        const AssetInfo *asset = FindEmbedAsset("vcap.html");
        K_ASSERT(asset);

        io->SendAsset(200, asset->data, "text/html", asset->compression_type);
    } else if (TestStr(request.path, "/save") && request.method == http_RequestMethod::Get) {
        if (!io->UpgradeToWS(0))
            return;

        int64_t now = GetUnixTime();
        TimeSpec spec = DecomposeTimeLocal(now);
        const char *filename = Fmt(io->Allocator(), "%1%/%2.webm", dest_directory, FmtTimeISO(spec)).ptr;

        StreamReader reader;
        StreamWriter writer;
        StreamWriter playback;

        io->OpenForReadWS(&reader);
        if (!writer.Open(filename))
            return;
        io->OpenForWriteWS(&playback);

        // Big WebSocket messages get truncated silently
        Span<uint8_t> buf = AllocateSpan<uint8_t>(io->Allocator(), Mebibytes(4));

        do {
            Size read_len = reader.Read(buf);
            if (read_len < 0)
                return;

            if (!playback.Write(buf.ptr, read_len))
                return;
            if (!writer.Write(buf.ptr, read_len))
                return;
        } while (!reader.IsEOF());
    } else {
        io->SendError(404);
    }
}

int Main(int argc, char **argv)
{
    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-D, --output_dir directory        Set output directory
                                   %!D..(default: %2)%!0

    %!..+-p, --port port%!0                Change web server port
                                   %!D..(default: %3)%!0
        %!..+--bind IP%!0                  Bind to specific IP)",
                FelixTarget, dest_directory, http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
        return 0;
    }

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-D", "--output_dir", OptionType::Value)) {
                dest_directory = opt.current_value;
            } else if (opt.Test("-p", "--port", OptionType::Value)) {
                if (!http.SetPortOrPath(opt.current_value))
                    return 1;
            } else if (opt.Test("--bind", OptionType::Value)) {
                http.bind_addr = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    LogInfo("Init HTTP server");

    http_Daemon daemon;
    if (!daemon.Bind(http))
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
        WaitResult ret = WaitEvents(-1);

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

    return status;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
