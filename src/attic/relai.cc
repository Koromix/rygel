// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"

#include <sys/socket.h>
#include <poll.h>

namespace K {

static int Connect(const char *host, int port)
{
    if (port >= 0) {
        SocketType type = strchr(host, ':') ? SocketType::IPv6 : SocketType::IPv4;

        int sock = CreateSocket(type, SOCK_STREAM);
        if (sock < 0)
            return -1;
        K_DEFER_N(err_guard) { CloseSocket(sock); };

        if (!ConnectIPSocket(sock, host, port))
            return -1;

        err_guard.Disable();
        return sock;
    } else {
        int sock = CreateSocket(SocketType::Unix, SOCK_STREAM);
        if (sock < 0)
            return -1;
        K_DEFER_N(err_guard) { CloseSocket(sock); };

        if (!ConnectUnixSocket(sock, host))
            return -1;

        err_guard.Disable();
        return sock;
    }
}

static Size Pump(int src, int dest)
{
    uint8_t buf[16384];
    Size bytes = read(src, buf, K_SIZE(buf));

    if (bytes < 0) {
        LogError("Failed to read: %1", strerror(errno));
        return -1;
    }
    if (!bytes)
        return 0;

    Span<uint8_t> remain = MakeSpan(buf, bytes);

    while (remain.len) {
        Size written = write(dest, remain.ptr, (size_t)remain.len);
        if (written < 0) {
            LogError("Failed to write: %1", strerror(errno));
            return -1;
        }

        remain.ptr += written;
        remain.len -= written;
    }

    return bytes;
}

int Main(int argc, char **argv)
{
    // Options
    const char *host = nullptr;
    int port = -1;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...] host port%!0
    %!..+%1 [option...] path%!0)", FelixTarget);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
        return 0;
    }

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        host = opt.ConsumeNonOption();

        if (const char *str = opt.ConsumeNonOption(); str) {
            if (!ParseInt(str, &port))
                return 1;
            if (port < 1 || port >= 65536) {
                LogError("Invalid TCP port %1", port);
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!host) {
        LogError("Missing host or UNIX socket path");
        return 1;
    }

    int sock = Connect(host, port);
    if (sock < 0)
        return 1;

    struct pollfd pfds[2] = {
        { 0, POLLIN, 0 },
        { -1, POLLIN, 0 },
    };

    for (;;) {
        bool connected = (sock >= 0);

        pfds[1].fd = sock;
        pfds[1].revents = 0;

        if (K_RESTART_EINTR(poll(pfds, 1 + connected, -1), < 0) < 0) {
            LogError("Failed to poll descriptors: %1", strerror(errno));
            return 1;
        }

        if (pfds[0].revents) {
            if (sock < 0) {
                sock = Connect(host, port);
                if (sock < 0)
                    return 1;
            }

            Size pumped = Pump(0, sock);

            if (pumped < 0)
                return 1;
            if (!pumped)
                break;
        }

        if (pfds[1].revents) {
            Size pumped = Pump(sock, 1);

            if (pumped < 0)
                return 1;
            if (!pumped) {
                close(sock);
                sock = -1;
            }
        }
    }

    if (sock >= 0) {
        shutdown(sock, SHUT_WR);

        for (;;) {
            Size pumped = Pump(sock, 1);

            if (pumped < 0)
                return 1;
            if (!pumped)
                break;
        }
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
