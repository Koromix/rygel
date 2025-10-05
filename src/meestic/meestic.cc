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
#include "config.hh"
#include "light.hh"
#include "src/tytools/libhs/libhs.h"
#include "src/core/sandbox/sandbox.hh"
#include "src/core/wrap/json.hh"

#if defined(__linux__)
    #include <poll.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <fcntl.h>
    #include <linux/input.h>
#endif

namespace K {

#if defined(__linux__)

static Config config;
static Size profile_idx = 0;
static bool transmit_info = false;

static hs_port *port = nullptr;

static bool ApplySandbox()
{
    sb_SandboxBuilder sb;

    if (!sb.Init())
        return false;

    sb.FilterSyscalls({
        { "exit", sb_FilterAction::Allow },
        { "exit_group", sb_FilterAction::Allow },
        { "brk", sb_FilterAction::Allow },
        { "mmap/anon", sb_FilterAction ::Allow},
        { "munmap", sb_FilterAction::Allow },
        { "mprotect/noexec", sb_FilterAction::Allow },
        { "madvise", sb_FilterAction::Allow },
        { "close", sb_FilterAction::Allow },
        { "fcntl", sb_FilterAction::Allow },
        { "read", sb_FilterAction::Allow },
        { "readv", sb_FilterAction::Allow },
        { "write", sb_FilterAction::Allow },
        { "writev", sb_FilterAction::Allow },
        { "pread64", sb_FilterAction::Allow },
        { "fsync", sb_FilterAction::Allow },
        { "poll", sb_FilterAction::Allow },
        { "ppoll", sb_FilterAction::Allow },
        { "clock_nanosleep", sb_FilterAction::Allow },
        { "clock_gettime", sb_FilterAction::Allow },
        { "clock_gettime64", sb_FilterAction::Allow },
        { "clock_nanosleep", sb_FilterAction::Allow },
        { "clock_nanosleep_time64", sb_FilterAction::Allow },
        { "nanosleep", sb_FilterAction::Allow },
        { "ioctl", sb_FilterAction::Allow },
        { "getpid", sb_FilterAction::Allow },
        { "accept", sb_FilterAction::Allow },
        { "accept4", sb_FilterAction::Allow },
        { "shutdown", sb_FilterAction::Allow },
        { "recv", sb_FilterAction::Allow },
        { "recvfrom", sb_FilterAction::Allow },
        { "recvmmsg", sb_FilterAction::Allow },
        { "recvmmsg_time64", sb_FilterAction::Allow },
        { "recvmsg", sb_FilterAction::Allow },
        { "sendmsg", sb_FilterAction::Allow },
        { "sendmmsg", sb_FilterAction::Allow },
        { "sendto", sb_FilterAction::Allow },
        { "rt_sigaction", sb_FilterAction::Allow },
        { "rt_sigpending", sb_FilterAction::Allow },
        { "rt_sigprocmask", sb_FilterAction::Allow },
        { "rt_sigqueueinfo", sb_FilterAction::Allow },
        { "rt_sigreturn", sb_FilterAction::Allow },
        { "rt_sigsuspend", sb_FilterAction::Allow },
        { "rt_sigtimedwait", sb_FilterAction::Allow },
        { "rt_sigtimedwait_time64", sb_FilterAction::Allow },
        { "kill", sb_FilterAction::Allow },
        { "tgkill", sb_FilterAction::Allow }
    });

    return sb.Apply();
}

static int OpenInputDevice(const char *needle, int flags)
{
    BlockAllocator temp_alloc;

    int ret_fd = -1;

    EnumerateDirectory("/dev/input", "event*", 1024, [&](const char *basename, FileType) {
        const char *filename = Fmt(&temp_alloc, "/dev/input/%1", basename).ptr;

        // Open device
        int fd = K_RESTART_EINTR(open(filename, O_RDONLY | O_CLOEXEC | flags), < 0);
        if (fd < 0) {
            LogError("Failed to open '%1': %2", filename, strerror(errno));
            return true;
        }
        K_DEFER_N(fd_guard) { close(fd); };

        char name[256];
        if (ioctl(fd, EVIOCGNAME(K_LEN(name)), name) < 0) {
            LogError("Failed to get device name of '%1': %2", filename, strerror(errno));
            return true;
        }

        if (TestStr(name, needle)) {
            ret_fd = fd;
            fd_guard.Disable();

            return false;
        }

        return true;
    });

    if (ret_fd < 0) {
        LogError("Cannot find input device '%1'", needle);
        return -1;
    }

    return ret_fd;
}

static bool ApplyProfile(Size idx)
{
    LogInfo("Applying profile %1", idx);

    if (port && !ApplyLight(port, config.profiles[idx].settings))
        return false;

    profile_idx = idx;
    transmit_info = true;

    return true;
}

static bool ToggleProfile(int delta)
{
    if (!delta)
        return true;

    Size next_idx = profile_idx;

    do {
        next_idx += delta;

        if (next_idx < 0) {
            next_idx = config.profiles.count - 1;
        } else if (next_idx >= config.profiles.count) {
            next_idx = 0;
        }
    } while (config.profiles[next_idx].manual);

    return ApplyProfile(next_idx);
}

static bool HandleInputEvent(int fd)
{
    struct input_event ev = {};
    Size len = read(fd, &ev, K_SIZE(ev));

    if (len < 0) {
        if (errno == EAGAIN)
            return true;

        LogError("Failed to read evdev event: %1", strerror(errno));
        return false;
    }
    K_ASSERT(len == K_SIZE(ev));

    if (ev.type == EV_KEY && ev.code == BTN_TRIGGER_HAPPY40 && ev.value == 1) {
        ToggleProfile(1);
    }

    return true;
}

static bool SendInfo(int sock, bool profiles)
{
    const auto write = [&](Span<const uint8_t> buf) {
        while (buf.len) {
            Size sent = send(sock, buf.ptr, (size_t)buf.len, 0);
            if (sent < 0) {
                LogError("Failed to send data to client: %1", strerror(errno));
                return false;
            }

            buf.ptr += sent;
            buf.len -= sent;
        }

        return true;
    };

    StreamWriter writer(write, "<client>");
    json_Writer json(&writer);

    json.StartObject();

    if (profiles) {
        json.Key("profiles"); json.StartArray();
        for (const ConfigProfile &profile: config.profiles) {
            json.String(profile.name);
        }
        json.EndArray();
    }
    json.Key("active"); json.Int(profile_idx);

    json.EndObject();

    if (!writer.Write('\n'))
        return false;

    return true;
}

static bool HandleClientData(int sock)
{
    BlockAllocator temp_alloc;

    const auto read = [sock](Span<uint8_t> out_buf) {
        struct pollfd pfd = { sock, POLLIN, 0 };
        int ret = poll(&pfd, 1, 1000);

        if (!ret) {
            LogError("Client has timed out");
            return (Size)-1;
        } else if (ret < 0) {
            LogError("poll() failed: %1", strerror(errno));
            return (Size)-1;
        }

        Size received = recv(sock, out_buf.ptr, out_buf.len, 0);
        if (received < 0) {
            LogError("Failed to receive data from client: %1", strerror(errno));
        }
        return received;
    };

    StreamReader reader(read, "<client>");
    json_Parser json(&reader, &temp_alloc);

    for (json.ParseObject(); json.InObject(); ) {
        Span<const char> key = json.ParseKey();

        if (key == "apply") {
            int64_t idx;
            if (!json.ParseInt(&idx))
                return false;
            if (idx < 0 || idx >= config.profiles.count) {
                LogError("Client asked for invalid profile");
                return false;
            }
            ApplyProfile(idx);
        } else if (key == "toggle") {
            Span<const char> type = {};
            if (!json.ParseString(&type))
                return false;

            if (type == "previous") {
                ToggleProfile(-1);
            } else if (type == "next") {
                ToggleProfile(1);
            } else {
                LogError("Invalid value '%1' for toggle command", type);
                return false;
            }
        } else {
            json.UnexpectedKey(key);
            return false;
        }
    }
    if (!json.IsValid())
        return false;

    return true;
}

static Size DoForClients(Span<WaitSource> sources, FunctionRef<bool(Size idx, int fd)> func)
{
    Size j = 2;
    for (Size i = 2; i < sources.len; i++) {
        sources[j] = sources[i];

        if (!func(i, sources[i].fd)) {
            close(sources[i].fd);
            continue;
        }

        j++;
    }
    return j;
}

static int RunDaemon(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Default config filename
    HeapArray<const char *> config_filenames;
    const char *config_filename = FindConfigFile("meestic.ini", &temp_alloc, &config_filenames);
    const char *socket_filename = "/run/meestic.sock";
    bool sandbox = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 daemon [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: see below)%!0
    %!..+-S, --socket_file socket%!0       Change control socket
                                   %!D..(default: %2)%!0

        %!..+--sandbox%!0                  Run in strict OS sandbox (if supported)

By default, the first of the following config files will be used:
)",
                FelixTarget, socket_filename);

        for (const char *filename: config_filenames) {
            PrintLn(st, "    %!..+%1%!0", filename);
        }
    };

    // Parse options
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/meestic.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.Test("-S", "--socket_file", OptionType::Value)) {
                socket_filename = opt.current_value;
            } else if (opt.Test("--sandbox")) {
                sandbox = true;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Parse config file
    if (config_filename && TestFile(config_filename, FileType::File)) {
        if (!LoadConfig(config_filename, &config))
            return 1;

        if (config.profiles.count >= 128) {
            LogError("Too many profiles (maximum = 128)");
            return 1;
        }

        profile_idx = config.default_idx;
    } else {
        AddDefaultProfiles(&config);
    }

    // Open the keyboard for Fn keys
    int input_fd = -1;
    if (GetDebugFlag("FAKE_KEYBOARD")) {
        static int pipe_fd[2];
        if (pipe2(pipe_fd, O_CLOEXEC) < 0) {
            LogError("pipe2() failed: %1", strerror(errno));
            return 1;
        }
        atexit([]() { close(pipe_fd[1]); });

        input_fd = pipe_fd[0];
    } else {
        input_fd = OpenInputDevice("AT Translated Set 2 keyboard", O_NONBLOCK);
        if (input_fd < 0)
            return 1;
    }
    K_DEFER { close(input_fd); };

    // Open the light MSI HID device ahead of time
    if (!GetDebugFlag("FAKE_LIGHTS")) {
        port = OpenLightDevice();
        if (!port)
            return 1;
    }
    K_DEFER { CloseLightDevice(port); };

    int listen_fd = CreateSocket(SocketType::Unix, SOCK_STREAM);
    if (listen_fd < 0)
        return 1;
    K_DEFER { close(listen_fd); };

    // Open control socket
    if (!BindUnixSocket(listen_fd, socket_filename))
        return 1;
    if (listen(listen_fd, 4) < 0) {
        LogError("listen() failed: %1", strerror(errno));
        return 1;
    }

    if (!NotifySystemd())
        return 1;

    if (sandbox && !ApplySandbox())
        return 1;

    // Check that it works once, at least
    if (!ApplyProfile(config.default_idx))
        return 1;
    transmit_info = false;

    // From here on, don't quit abruptly
    WaitEvents(0);

    // Wait for events and clients
    int status = 0;
    {
        LocalArray<WaitSource, 32> sources;

        sources.Append({ input_fd, POLLIN, -1 });
        sources.Append({ listen_fd, POLLIN, -1 });

        for (;;) {
            uint64_t ready = 0;
            WaitResult ret = WaitEvents(sources, -1, &ready);

            if (ret == WaitResult::Exit) {
                LogInfo("Exit requested");
                break;
            } else if (ret == WaitResult::Interrupt) {
                LogInfo("Process interrupted");
                status = 1;
                break;
            } else if (ret == WaitResult::Error) {
                status = 1;
                break;
            }

            // Handle input events
            if (ready & 1) {
                if (!HandleInputEvent(input_fd))
                    return 1;
            }

            // Accept new clients
            if (ready & 2) {
                int sock = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);

                if (sock >= 0) {
                    if (sources.Available()) {
                        sources.Append({ sock, POLLIN, -1 });
                        SendInfo(sock, true);
                    } else {
                        LogError("Cannot handle new client (too many)");
                        CloseSocket(sock);
                    }
                } else {
                    LogError("Failed to accept new client: %1", strerror(errno));
                }
            }

            // Handle client data
            sources.len = DoForClients(sources, [&](Size idx, int sock) {
                bool process = (ready & (1u << idx));
                return process ? HandleClientData(sock) : true;
            });

            // Send updates
            if (transmit_info) {
                sources.len = DoForClients(sources, [&](Size, int sock) { return SendInfo(sock, false); });
                transmit_info = false;
            }
        }
    }

    return status;
}

#endif

static int RunSet(Span<const char *> arguments)
{
    // Options
    LightSettings settings;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 set [option...] [color...]%!0

Options:

    %!..+-m, --mode mode%!0                Set light mode (see below)
                                   %!D..(default: %2)%!0
    %!..+-s, --speed speed%!0              Set speed of change, from 0 and 2
                                   %!D..(default: %3)%!0
    %!..+-i, --intensity intensity%!0      Set light intensity, from 0 to 10
                                   %!D..(default: %4)%!0

Supported modes:
)"), FelixTarget, LightModeOptions[(int)settings.mode].name, settings.speed, settings.intensity);
        for (const OptionDesc &desc: LightModeOptions) {
            PrintLn(st, "    %!..+%1%!0    %2", FmtArg(desc.name).Pad(27), T(desc.help));
        };
        PrintLn(st, T(R"(
A few predefined color names can be used (such as MsiBlue), or you can use
hexadecimal RGB color codes. Don't forget the quotes or your shell may not
like the hash character.

Predefined color names:
)"));
        for (const PredefinedColor &color: PredefinedColors) {
            PrintLn(st, "    %!..+%1%!0    %!D..#%2%3%4%!0", FmtArg(color.name).Pad(27), FmtHex(color.rgb.red).Pad0(-2),
                                                                                       FmtHex(color.rgb.green).Pad0(-2),
                                                                                       FmtHex(color.rgb.blue).Pad0(-2));
        };
        PrintLn(st, T(R"(
Examples:

    Disable lighting
    %!..+%1 -m Disabled%!0

    Set default static MSI blue
    %!..+%1 -m Static MsiBlue%!0

    Slowly breathe between Orange and MsiBlue
    %!..+%1 -m Breathe -s 0 "#FFA100" MsiBlue%!0

    Quickly transition between Magenta, Orange and MsiBlue colors
    %!..+%1 -m Transition -s 2 Magenta Orange MsiBlue%!0

Be careful, color names and most options are %!..+case-sensitive%!0.)"), FelixTarget);
    };

    // Harmonize log output
    hs_log_set_handler([](hs_log_level level, int, const char *msg, void *) {
        switch (level) {
            case HS_LOG_ERROR:
            case HS_LOG_WARNING: { LogError(("%1"), msg); } break;
            case HS_LOG_DEBUG: { LogDebug(("%1"), msg); } break;
        }
    }, nullptr);

    // Parse options
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-m", "--mode", OptionType::Value)) {
                if (!OptionToEnumI(LightModeOptions, opt.current_value, &settings.mode)) {
                    LogError("Invalid mode '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-s", "--speed", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &settings.speed))
                    return 1;
            } else if (opt.Test("-i", "--intensity", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &settings.intensity))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *arg;
        while ((arg = opt.ConsumeNonOption())) {
            RgbColor color;
            if (!ParseColor(arg, &color))
                return 1;

            if (!settings.colors.Available()) {
                LogError("A maximum of %1 colors is supported", K_LEN(settings.colors.data));
                return 1;
            }
            settings.colors.Append(color);
        }
    }

    if (!GetDebugFlag("FAKE_LIGHTS")) {
        if (!ApplyLight(settings))
            return 1;
    }

    LogInfo("Done!");
    return 0;
}

int Main(int argc, char **argv)
{
    InitLocales(TranslationTables);

    // Handle help and version arguments
    if (argc >= 2) {
        if (TestStr(argv[1], "--help") || TestStr(argv[1], "help")) {
            if (argc >= 3 && argv[2][0] != '-') {
                argv[1] = argv[2];
                argv[2] = const_cast<char *>("--help");
            } else {
                const char *args[] = {"--help"};
                return RunSet(args);
            }
        } else if (TestStr(argv[1], "--version")) {
            PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
            PrintLn(T("Compiler: %1"), FelixCompiler);

            return 0;
        }
    }

    const char *cmd = nullptr;
    Span<const char *> arguments = {};

    if (argc >= 2) {
        cmd = argv[1];

        if (cmd[0] == '-') {
            cmd = "set";
            arguments = MakeSpan((const char **)argv + 1, argc - 1);
        } else {
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        }
    } else {
        cmd = "set";
    }

    if (TestStr(cmd, "set")) {
        return RunSet(arguments);
#if defined(__linux__)
    } else if (TestStr(cmd, "daemon")) {
        return RunDaemon(arguments);
#endif
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
