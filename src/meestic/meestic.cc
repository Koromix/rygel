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
#include "config.hh"
#include "lights.hh"
#include "src/core/libsandbox/libsandbox.hh"
#include "vendor/libhs/libhs.h"

#ifdef __linux__
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/poll.h>
    #include <sys/socket.h>
    #include <fcntl.h>
    #include <linux/input.h>
#endif

// Allows some tests without the MSI Delta 15
// #define FAKE_KEYBOARD
// #define FAKE_LIGHTS

namespace RG {

#ifdef __linux__

static Config config;
static Size profile_idx = 0;

static hs_port *port = nullptr;

static bool ApplySandbox()
{
    if (!sb_IsSandboxSupported()) {
        LogError("Sandbox mode is not supported on this platform");
        return false;
    }

    sb_SandboxBuilder sb;

    sb.FilterSyscalls(sb_FilterAction::Kill, {
        {"exit", sb_FilterAction::Allow},
        {"exit_group", sb_FilterAction::Allow},
        {"brk", sb_FilterAction::Allow},
        {"mmap/anon", sb_FilterAction::Allow},
        {"munmap", sb_FilterAction::Allow},
        {"close", sb_FilterAction::Allow},
        {"fcntl", sb_FilterAction::Allow},
        {"read", sb_FilterAction::Allow},
        {"readv", sb_FilterAction::Allow},
        {"write", sb_FilterAction::Allow},
        {"writev", sb_FilterAction::Allow},
        {"pread64", sb_FilterAction::Allow},
        {"fsync", sb_FilterAction::Allow},
        {"poll", sb_FilterAction::Allow},
        {"ppoll", sb_FilterAction::Allow},
        {"clock_nanosleep", sb_FilterAction::Allow},
        {"clock_gettime", sb_FilterAction::Allow},
        {"clock_gettime64", sb_FilterAction::Allow},
        {"clock_nanosleep", sb_FilterAction::Allow},
        {"clock_nanosleep_time64", sb_FilterAction::Allow},
        {"nanosleep", sb_FilterAction::Allow},
        {"ioctl", sb_FilterAction::Allow},
        {"getpid", sb_FilterAction::Allow},
        {"accept", sb_FilterAction::Allow},
        {"accept4", sb_FilterAction::Allow},
        {"shutdown", sb_FilterAction::Allow},
        {"recv", sb_FilterAction::Allow},
        {"recvfrom", sb_FilterAction::Allow},
        {"recvmmsg", sb_FilterAction::Allow},
        {"recvmmsg_time64", sb_FilterAction::Allow},
        {"recvmsg", sb_FilterAction::Allow},
        {"sendmsg", sb_FilterAction::Allow},
        {"sendmmsg", sb_FilterAction::Allow},
        {"sendto", sb_FilterAction::Allow},
        {"rt_sigaction", sb_FilterAction::Allow},
        {"rt_sigpending", sb_FilterAction::Allow},
        {"rt_sigprocmask", sb_FilterAction::Allow},
        {"rt_sigqueueinfo", sb_FilterAction::Allow},
        {"rt_sigreturn", sb_FilterAction::Allow},
        {"rt_sigsuspend", sb_FilterAction::Allow},
        {"rt_sigtimedwait", sb_FilterAction::Allow},
        {"rt_sigtimedwait_time64", sb_FilterAction::Allow},
        {"kill", sb_FilterAction::Allow},
        {"tgkill", sb_FilterAction::Allow}
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
        int fd = open(filename, O_RDONLY | O_CLOEXEC | flags);
        if (fd < 0) {
            LogError("Failed to open '%1': %2", filename, strerror(errno));
            return true;
        }
        RG_DEFER_N(fd_guard) { close(fd); };

        char name[256];
        if (ioctl(fd, EVIOCGNAME(RG_LEN(name)), name) < 0) {
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

#ifndef FAKE_LIGHTS
    if (!ApplyLight(port, config.profiles[idx].settings))
        return false;
#endif

    profile_idx = idx;
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
            next_idx = config.profiles.len - 1;
        } else if (next_idx >= config.profiles.len) {
            next_idx = 0;
        }
    } while (config.profiles[next_idx].manual);

    return ApplyProfile(next_idx);
}

static bool HandleInputEvent(int fd)
{
    struct input_event ev = {};
    Size len = read(fd, &ev, RG_SIZE(ev));

    if (len < 0) {
        if (errno == EAGAIN)
            return true;

        LogError("Failed to read evdev event: %1", strerror(errno));
        return false;
    }
    RG_ASSERT(len == RG_SIZE(ev));

    if (ev.type == EV_KEY && ev.code == BTN_TRIGGER_HAPPY40 && ev.value == 1) {
        ToggleProfile(1);
    }

    return true;
}

static bool HandleClientConnect(int fd)
{
    const auto write = [&](Span<const uint8_t> buf) {
        while (buf.len) {
            Size sent = send(fd, buf.ptr, (size_t)buf.len, 0);
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

    for (const ConfigProfile &profile: config.profiles) {
        PrintLn(&writer, "[%1]", profile.name);

        PrintLn(&writer, "Mode = %1", LightModeOptions[(int)profile.settings.mode].name);
        PrintLn(&writer, "Speed = %1", profile.settings.speed);
        PrintLn(&writer, "Intensity = %1", profile.settings.intensity);

        Print(&writer, "Colors =");
        for (RgbColor color: profile.settings.colors) {
            Print(&writer, " #%1%2%3", FmtHex(color.red).Pad0(-2), FmtHex(color.blue).Pad0(-2), FmtHex(color.green).Pad0(-2));
        }
        PrintLn(&writer);

        PrintLn(&writer);
    }

    // Signal EOF (full config)
    shutdown(fd, SHUT_WR);

    return writer.IsValid();
}

static bool HandleClientMessage(int fd)
{
    uint8_t payload;
    Size received = recv(fd, &payload, RG_SIZE(payload), 0);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;

        LogError("Failed to read client data");
        return false;
    } else if (!received) {
        return false;
    }

    if (payload < 0x80) {
        Size idx = payload;

        if (idx >= config.profiles.len) {
            LogError("Client asked for invalid profile");
            return false;
        }
        if (!ApplyProfile(idx))
            return false;
    } else if (payload == 0x80) {
        if (!ToggleProfile(-1))
            return false;
    } else if (payload == 0x81) {
        if (!ToggleProfile(1))
            return false;
    } else {
        LogError("Invalid command 0x%1 from client", FmtHex(payload));
        return false;
    }

    return true;
}

static int RunDaemon(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Default config filename
    LocalArray<const char *, 4> config_filenames;
    const char *config_filename = FindConfigFile("MeesticGui.ini", &temp_alloc, &config_filenames);
    const char *socket_filename = "/run/meestic.sock";
    bool sandbox = true;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 daemon [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0
    %!..+-S, --socket_file <socket>%!0   Change control socket
                                 %!D..(default: %3)%!0

        %!..+--no_sandbox%!0             Disable use of sandboxing)",
                FelixTarget, FmtSpan(config_filenames.As()), socket_filename);
    };

    // Parse options
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/meestic.ini", TrimStrRight(opt.current_value, RG_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.Test("-S", "--socket_file", OptionType::Value)) {
                socket_filename = opt.current_value;
            } else if (opt.Test("--no_sandbox")) {
                sandbox = false;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Parse config file
    if (config_filename && TestFile(config_filename, FileType::File)) {
        if (!LoadConfig(config_filename, &config))
            return 1;

        if (config.profiles.len >= 128) {
            LogError("Too many profiles (maximum = 128)");
            return 1;
        }

        profile_idx = config.default_idx;
    } else {
        AddDefaultProfiles(&config);
    }

    // Open the keyboard for Fn keys
#ifdef FAKE_KEYBOARD
    int pipe_fd[2];
    if (pipe2(pipe_fd, O_CLOEXEC) < 0) {
        LogError("pipe2() failed: %1", strerror(errno));
        return 1;
    }
    RG_DEFER {
        close(pipe_fd[1]);
        close(pipe_fd[0]);
    };

    int input_fd = pipe_fd[0];
#else
    int input_fd = OpenInputDevice("AT Translated Set 2 keyboard", O_NONBLOCK);
    if (input_fd < 0)
        return 1;
#endif

    // Open the light MSI HID device ahead of time
#ifndef FAKE_LIGHTS
    port = OpenLightDevice();
    if (!port)
        return 1;
    RG_DEFER { CloseLightDevice(port); };
#endif

    // Open control socket
    int listen_fd = OpenUnixSocket(socket_filename);
    if (listen_fd < 0)
        return 1;
    RG_DEFER { close(listen_fd); };

    // Listen for clients
    if (listen(listen_fd, 4) < 0) {
        LogError("listen() failed: %1", strerror(errno));
        return 1;
    }

    if (sandbox && !ApplySandbox())
        return 1;

    // Check that it works once, at least
    if (!ApplyProfile(config.default_idx))
        return 1;

    // From here on, don't quit abruptly
    WaitForInterrupt(0);

    // Wait for events and clients
    {
        HeapArray<struct pollfd> pfds;

        pfds.Append({ input_fd, POLLIN, 0 });
        pfds.Append({ listen_fd, POLLIN, 0 });

        for (;;) {
            if (poll(pfds.ptr, pfds.len, -1) < 0) {
                if (errno == EINTR) {
                    if (WaitForResult(0) == WaitForResult::Interrupt) {
                        LogInfo("Exit requested");
                        break;
                    } else {
                        continue;
                    }
                }

                LogError("Failed to poll I/O descriptors: %1", strerror(errno));
                return 1;
            }

            // Handle input events
            if (pfds[0].revents && !HandleInputEvent(input_fd))
                return 1;

            // Accept new clients
            if (pfds[1].revents) {
                int fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);

                if (fd >= 0) {
                    if (HandleClientConnect(fd)) {
                        pfds.Append({ fd, POLLIN, 0 });
                    } else {
                        close(fd);
                    }
                } else {
                    LogError("Failed to accept new client: %1", strerror(errno));
                }
            }

            // Handle existing clients
            {
                Size j = 2;
                for (Size i = 2; i < pfds.len; i++) {
                    pfds[j] = pfds[i];

                    bool keep = true;
                    if (pfds[i].revents & POLLIN) {
                        keep &= HandleClientMessage(pfds[i].fd);
                    }
                    if (!keep) {
                        close(pfds[i].fd);
                        continue;
                    }

                    j++;
                }
                pfds.len = j;
            }
        }
    }

    return 0;
}

#endif

static int RunChange(Span<const char *> arguments)
{
    // Options
    LightSettings settings;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 change [options...] [colors...]%!0

Options:
    %!..+-m, --mode <mode>%!0            Set light mode (see below)
                                 %!D..(default: %2)%!0
    %!..+-s, --speed <speed>%!0          Set speed of change, from 0 and 2
                                 %!D..(default: %3)%!0
    %!..+-i, --intensity <intensity>%!0  Set light intensity, from 0 to 10
                                 %!D..(default: %4)%!0

Supported modes:)", FelixTarget, LightModeOptions[(int)settings.mode].name, settings.speed, settings.intensity);
        for (const OptionDesc &desc: LightModeOptions) {
            PrintLn(fp, "    %!..+%1%!0  %2", FmtArg(desc.name).Pad(27), desc.help);
        };
        PrintLn(fp, R"(
A few predefined color names can be used (such as MsiBlue), or you can use
hexadecimal RGB color codes. Don't forget the quotes or your shell may not
like the hash character.

Predefined color names:)");
        for (const PredefinedColor &color: PredefinedColors) {
            PrintLn(fp, "    %!..+%1%!0  %!D..#%2%3%4%!0", FmtArg(color.name).Pad(27), FmtHex(color.rgb.red).Pad0(-2),
                                                                                       FmtHex(color.rgb.green).Pad0(-2),
                                                                                       FmtHex(color.rgb.blue).Pad0(-2));
        };
        PrintLn(fp, R"(
Examples:
    Disable lighting
    %!..+%1 -m Disabled%!0

    Set default static MSI blue
    %!..+%1 -m Static MsiBlue%!0

    Slowly breathe between Orange and MsiBlue
    %!..+%1 -m Breathe -s 0 "#FFA100" MsiBlue%!0

    Quickly transition between Magenta, Orange and MsiBlue colors
    %!..+%1 -m Transition -s 2 Magenta Orange MsiBlue%!0

Be careful, color names and most options are %!..+case-sensitive%!0.)", FelixTarget);
    };

    // Harmonize log output
    hs_log_set_handler([](hs_log_level level, int, const char *msg, void *) {
        switch (level) {
            case HS_LOG_ERROR:
            case HS_LOG_WARNING: { LogError("%1", msg); } break;
            case HS_LOG_DEBUG: { LogDebug("%1", msg); } break;
        }
    }, nullptr);

    // Parse options
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-m", "--mode", OptionType::Value)) {
                if (!OptionToEnum(LightModeOptions, opt.current_value, &settings.mode)) {
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

        for (;;) {
            const char *arg = opt.ConsumeNonOption();
            if (!arg)
                break;

            RgbColor color;
            if (!ParseColor(arg, &color))
                return 1;

            if (!settings.colors.Available()) {
                LogError("A maximum of %1 colors is supported", RG_LEN(settings.colors.data));
                return 1;
            }
            settings.colors.Append(color);
        }
    }

#ifndef FAKE_LIGHTS
    if (!ApplyLight(settings))
        return 1;
#endif

    LogInfo("Done!");
    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Handle help and version arguments
    if (argc >= 2) {
        if (TestStr(argv[1], "--help") || TestStr(argv[1], "help")) {
            if (argc >= 3 && argv[2][0] != '-') {
                argv[1] = argv[2];
                argv[2] = const_cast<char *>("--help");
            } else {
                const char *args[] = {"--help"};
                return RunChange(args);
            }
        } else if (TestStr(argv[1], "--version")) {
            PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
            PrintLn("Compiler: %1", FelixCompiler);

            return 0;
        }
    }

    const char *cmd = nullptr;
    Span<const char *> arguments = {};

    if (argc >= 2) {
        cmd = argv[1];

        if (cmd[0] == '-') {
            cmd = "change";
            arguments = MakeSpan((const char **)argv + 1, argc - 1);
        } else {
            arguments = MakeSpan((const char **)argv + 2, argc - 2);
        }
    } else {
        cmd = "change";
    }

    if (TestStr(cmd, "change")) {
        return RunChange(arguments);
#ifdef __linux__
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
int main(int argc, char **argv) { return RG::Main(argc, argv); }
