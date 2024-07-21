// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#if defined(_WIN32)

#include "src/core/base/base.hh"
#include "server.hh"
#include "misc.hh"

#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <io.h>

#if !defined(UNIX_PATH_MAX)
    #define UNIX_PATH_MAX 108
#endif
typedef struct sockaddr_un {
    ADDRESS_FAMILY sun_family;
    char sun_path[UNIX_PATH_MAX];
} SOCKADDR_UN, *PSOCKADDR_UN;

namespace RG {

static const Size AcceptAddressLen = 2 * sizeof(SOCKADDR_STORAGE) + 16;

static const int MinSockets = 16;
static const int MaxSockets = 128;
static const int WorkersPerDispatcher = 4;

enum class PendingOperation {
    None,
    Accept,
    Disconnect,
    WantDisconnect,
    Read
};

struct SocketData {
    int sock = -1;
    bool connected = false;

    PendingOperation op = PendingOperation::None;
    OVERLAPPED overlapped = {};
    uint8_t accept[2 * AcceptAddressLen];

    std::unique_ptr<http_IO> client = nullptr;

    ~SocketData() { closesocket(sock); };
};

struct IndirectFunctions {
    LPFN_ACCEPTEX AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs;
    LPFN_DISCONNECTEX DisconnectEx;
};

class http_Daemon::Dispatcher {
    RG_DELETE_COPY(Dispatcher);

    http_Daemon *daemon;
    HANDLE iocp;
    IndirectFunctions fn;

    HeapArray<SocketData *> sockets;
    HeapArray<SocketData *> free_sockets;

    LocalArray<http_IO *, 256> free_clients;

public:
    Dispatcher(http_Daemon *daemon, HANDLE iocp, IndirectFunctions fn) : daemon(daemon), iocp(iocp), fn(fn) {}
    ~Dispatcher() { Close(); }

    bool Init();
    bool Run();

private:
    bool PostAccept();
    bool PostRead(SocketData *socket);

    SocketData *InitSocket();
    void DisconnectSocket(SocketData *socket);
    void DestroySocket(SocketData *socket);

    http_IO *InitClient(int fd, int64_t start, struct sockaddr *sa);
    void ParkClient(http_IO *client);

    void Close();
};

bool http_Daemon::Bind(const http_Config &config, bool log_addr)
{
    RG_ASSERT(listen_fd < 0);

    if (!InitConfig(config))
        return false;

    switch (config.sock_type) {
        case SocketType::Dual:
        case SocketType::IPv4:
        case SocketType::IPv6: { listen_fd = OpenIPSocket(config.sock_type, config.port, SOCK_STREAM | SOCK_OVERLAPPED); } break;
        case SocketType::Unix: { listen_fd = OpenUnixSocket(config.unix_path, SOCK_STREAM | SOCK_OVERLAPPED); } break;
    }
    if (listen_fd < 0)
        return false;

    if (listen(listen_fd, 256) < 0) {
        errno = TranslateWinSockError();

        LogError("Failed to listen on socket: %1", strerror(errno));
        return false;
    }

    if (log_addr) {
        if (config.sock_type == SocketType::Unix) {
            LogInfo("Listening on socket '%!..+%1%!0' (Unix stack)", config.unix_path);
        } else {
            LogInfo("Listening on %!..+http://localhost:%1/%!0 (%2 stack)",
                    config.port, SocketTypeNames[(int)config.sock_type]);
        }
    }

    return true;
}

bool http_Daemon::Start(std::function<void(const http_RequestInfo &request, http_IO *io)> func)
{
    RG_ASSERT(listen_fd >= 0);
    RG_ASSERT(!handle_func);
    RG_ASSERT(func);

    RG_DEFER_N(err_guard) {
        if (async) {
            delete async;
            async = nullptr;
        }

        if (iocp) {
            CloseHandle(iocp);
            iocp = nullptr;
        }

        for (Dispatcher *dispatcher: dispatchers) {
            delete dispatcher;
        }
        dispatchers.Clear();
    };

    // Heuristic found on MDN
    async = new Async(1 + 2 * GetCoreCount());

    iocp = CreateIoCompletionPort((HANDLE)(uintptr_t)listen_fd, nullptr, 1, 0);
    if (!iocp) {
        LogError("Failed to create I/O completion port: %1", GetWin32ErrorString());
        return false;
    }

    IndirectFunctions fn;
    {
        static const GUID AcceptExGuid = WSAID_ACCEPTEX;
        static const GUID GetAcceptExSockaddrsGuid = WSAID_GETACCEPTEXSOCKADDRS;
        static const GUID DisconnectExGuid = WSAID_DISCONNECTEX;

        DWORD dummy;

        if (WSAIoctl((SOCKET)listen_fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     (void *)&AcceptExGuid, RG_SIZE(AcceptExGuid), 
                     &fn.AcceptEx, RG_SIZE(fn.AcceptEx),
                     &dummy, nullptr, nullptr) == SOCKET_ERROR) {
            errno = TranslateWinSockError();

            LogError("Failed to load AcceptEx() function: %1", strerror(errno));
            return false;
        }

        if (WSAIoctl((SOCKET)listen_fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     (void *)&GetAcceptExSockaddrsGuid, RG_SIZE(GetAcceptExSockaddrsGuid), 
                     &fn.GetAcceptExSockaddrs, RG_SIZE(fn.GetAcceptExSockaddrs),
                     &dummy, nullptr, nullptr) == SOCKET_ERROR) {
            errno = TranslateWinSockError();

            LogError("Failed to load GetAcceptExSockaddrs() function: %1", strerror(errno));
            return false;
        }

        if (WSAIoctl((SOCKET)listen_fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     (void *)&DisconnectExGuid, RG_SIZE(DisconnectExGuid), 
                     &fn.DisconnectEx, RG_SIZE(fn.DisconnectEx),
                     &dummy, nullptr, nullptr) == SOCKET_ERROR) {
            errno = TranslateWinSockError();

            LogError("Failed to load DisconnectEx() function: %1", strerror(errno));
            return false;
        }
    }

    for (Size i = 1; i < async->GetWorkerCount(); i++) {
        Dispatcher *dispatcher = new Dispatcher(this, iocp, fn);
        dispatchers.Append(dispatcher);

        if (!dispatcher->Init())
            return false;
    }

    // Can fail anymore
    err_guard.Disable();

    handle_func = func;

    for (Dispatcher *dispatcher: dispatchers) {
        async->Run([=] { return dispatcher->Run(); });
    }

    return true;
}

void http_Daemon::Stop()
{
    for (Size i = 0; i < dispatchers.len; i++) {
        PostQueuedCompletionStatus(iocp, 0, 0, nullptr);
    }

    if (async) {
        async->Sync();

        delete async;
        async = nullptr;
    }

    for (Dispatcher *dispatcher: dispatchers) {
        delete dispatcher;
    }
    dispatchers.Clear();

    CloseSocket(listen_fd);
    listen_fd = -1;

    if (iocp) {
        CloseHandle(iocp);
        iocp = nullptr;
    }

    handle_func = {};
}

static SocketData *SocketFromOverlapped(void *ptr)
{
    uint8_t *data = (uint8_t *)ptr;
    return (SocketData *)(data - offsetof(SocketData, overlapped));
}

bool http_Daemon::Dispatcher::Init()
{
    RG_DEFER_N(err_guard) { Close(); };

    for (Size i = 0; i < MinSockets; i++) {
        if (!PostAccept())
            return false;
    }

    err_guard.Disable();
    return true;
}

bool http_Daemon::Dispatcher::Run()
{
    Async async(1 + WorkersPerDispatcher);

    RG_DEFER {
        async.Sync();
        Close();
    };

    int next_worker = 0;

    for (;;) {
        DWORD transferred;
        uintptr_t key;
        OVERLAPPED *overlapped;

        bool success = GetQueuedCompletionStatus(iocp, &transferred, &key, &overlapped, INFINITE);

        if (!success && !overlapped) {
            LogError("GetQueuedCompletionStatus() failed: %1", GetWin32ErrorString());
            return false;
        }

        // Exit signal
        if (!key) [[unlikely]] {
            RG_ASSERT(success);
            return true;
        }

        int64_t now = GetMonotonicTime();
        SocketData *socket = SocketFromOverlapped(overlapped);
        PendingOperation op = socket->op;

        socket->op = PendingOperation::None;

        switch (op) {
            case PendingOperation::None: {} break;

            case PendingOperation::Accept: {
                PostAccept();

                if (!success) {
                    DestroySocket(socket);
                    continue;
                }

                socket->connected = true;
                setsockopt(socket->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&daemon->listen_fd, RG_SIZE(daemon->listen_fd));

                sockaddr *local_addr = nullptr;
                sockaddr *remote_addr = nullptr;
                int local_len;
                int remote_len;

                fn.GetAcceptExSockaddrs(socket->accept, 0, AcceptAddressLen, AcceptAddressLen,
                                        &local_addr, &local_len, &remote_addr, &remote_len);

                http_IO *client = InitClient(socket->sock, now, remote_addr);

                socket->client.reset(client);

                if (!PostRead(socket)) {
                    DisconnectSocket(socket);
                    continue;
                }
            } break;

            case PendingOperation::Disconnect: {
                if (!success) [[unlikely]] {
                    DestroySocket(socket);
                    continue;
                }

                socket->connected = false;
                free_sockets.Append(socket);
            } break;

            case PendingOperation::WantDisconnect: {
                RG_ASSERT(success);
                DisconnectSocket(socket);
            } break;

            case PendingOperation::Read: {
                if (!success) [[unlikely]] {
                    DisconnectSocket(socket);
                    continue;
                }

                http_IO *client = socket->client.get();

                client->incoming.buf.len += (Size)transferred;
                client->incoming.buf.ptr[client->incoming.buf.len] = 0;

                http_IO::PrepareStatus status = socket->client->Prepare(now);

                switch (status) {
                    case http_IO::PrepareStatus::Incoming: {
                        if (!PostRead(socket)) {
                            DisconnectSocket(socket);
                        }
                    } break;

                    case http_IO::PrepareStatus::Ready: {
                        if (!client->InitAddress()) {
                            client->request.keepalive = false;
                            client->SendError(400);
                            DisconnectSocket(socket);

                            break;
                        }

                        client->request.keepalive &= (now < client->socket_start + daemon->keepalive_time);

                        int worker_idx = 1 + next_worker;
                        next_worker = (next_worker + 1) % WorkersPerDispatcher;

                        if (client->request.keepalive) {
                            async.Run(worker_idx, [=, this] {
                                daemon->RunHandler(client);

                                client->Rearm(now);

                                if (!PostRead(socket)) {
                                    socket->op = PendingOperation::WantDisconnect;
                                    PostQueuedCompletionStatus(iocp, 0, 1, &socket->overlapped);
                                }

                                return true;
                            });
                        } else {
                            async.Run(worker_idx, [=, this] {
                                daemon->RunHandler(client);

                                socket->op = PendingOperation::WantDisconnect;
                                PostQueuedCompletionStatus(iocp, 0, 1, &socket->overlapped);

                                return true;
                            });
                        }
                    } break;

                    case http_IO::PrepareStatus::Busy: {} break;

                    case http_IO::PrepareStatus::Close: { DisconnectSocket(socket); } break;

                    case http_IO::PrepareStatus::Unused: { RG_UNREACHABLE(); } break;
                }
            } break;
        }
    }

    RG_UNREACHABLE();
}

bool http_Daemon::Dispatcher::PostAccept()
{
    SocketData *socket = InitSocket();
    if (!socket)
        return false;
    RG_DEFER_N(err_guard) { DisconnectSocket(socket); };

    DWORD received = 0;

retry:
    if (!fn.AcceptEx((SOCKET)daemon->listen_fd, (SOCKET)socket->sock, socket->accept, 0,
                     AcceptAddressLen, AcceptAddressLen, &received, &socket->overlapped) &&
            WSAGetLastError() != ERROR_IO_PENDING) {
        errno = TranslateWinSockError();

        if (errno == ECONNRESET)
            goto retry;

        LogError("Failed to issue socket accept operation: %1", strerror(errno));
        return false;
    }

    socket->op = PendingOperation::Accept;
    err_guard.Disable();

    return true;
}

bool http_Daemon::Dispatcher::PostRead(SocketData *socket)
{
    RG_ASSERT(socket->op == PendingOperation::None);
    RG_ASSERT(socket->client);

    http_IO *client = socket->client.get();

    client->incoming.buf.Grow(Mebibytes(1));

    WSABUF buf = { (unsigned long)client->incoming.buf.Available() - 1, (char *)client->incoming.buf.end() };
    DWORD received = 0;
    DWORD flags = 0;

    if (WSARecv((SOCKET)socket->sock, &buf, 1, &received, &flags, &socket->overlapped, nullptr) &&
            WSAGetLastError() != ERROR_IO_PENDING) {
        errno = TranslateWinSockError();

        if (errno != ECONNRESET) {
            LogError("Failed to read from socket: %1", strerror(errno));
        }

        return false;
    }

    socket->op = PendingOperation::Read;

    return true;
}

SocketData *http_Daemon::Dispatcher::InitSocket()
{
    SocketData *socket = nullptr;

    if (free_sockets.len) {
        int idx = GetRandomInt(0, free_sockets.len);

        socket = free_sockets[idx];

        std::swap(free_sockets[idx], free_sockets[free_sockets.len - 1]);
        free_sockets.len--;
    } else {
        socket = new SocketData();
        RG_DEFER_N(err_guard) { delete socket; };

        socket->sock = CreateSocket(daemon->sock_type, SOCK_STREAM | SOCK_OVERLAPPED);
        if (socket->sock < 0)
            return nullptr;

        if (!CreateIoCompletionPort((HANDLE)(uintptr_t)socket->sock, iocp, 1, 0)) {
            LogError("Failed to associate socket with IOCP: %1", GetWin32ErrorString());
            return nullptr;
        }

        err_guard.Disable();
        sockets.Append(socket);
    }

    return socket;
}

void http_Daemon::Dispatcher::DisconnectSocket(SocketData *socket)
{
    RG_ASSERT(socket->op == PendingOperation::None);

    if (socket->connected && !fn.DisconnectEx((SOCKET)socket->sock, &socket->overlapped, TF_REUSE_SOCKET, 0)) {
        if (WSAGetLastError() == ERROR_IO_PENDING) {
            socket->op = PendingOperation::Disconnect;
            return;
        }

        errno = TranslateWinSockError();

        LogError("Failed to reuse socket: %1", strerror(errno));
        DestroySocket(socket);
    }

    if (socket->client) {
        http_IO *client = socket->client.release();
        ParkClient(client);
    }

    free_sockets.Append(socket);
}

void http_Daemon::Dispatcher::DestroySocket(SocketData *socket)
{
    // Filter it out from our array of reusable sockets
    Size j = 0;
    for (Size i = 0; i < sockets.len; i++) {
        sockets[j] = sockets[i];
        j += (sockets[i] != socket);
    }
    sockets.len = j;

    delete socket;
}

http_IO *http_Daemon::Dispatcher::InitClient(int fd, int64_t start, struct sockaddr *sa)
{
    http_IO *client = nullptr;

    if (free_clients.len) {
        int idx = GetRandomInt(0, free_clients.len);

        client = free_clients[idx];

        std::swap(free_clients[idx], free_clients[free_clients.len - 1]);
        free_clients.len--;
    } else {
        client = new http_IO(daemon);
    }

    if (!client->Init(fd, start, sa)) [[unlikely]] {
        delete client;
        return nullptr;
    }

    return client;
}

void http_Daemon::Dispatcher::ParkClient(http_IO *client)
{
    if (free_clients.Available()) {
        free_clients.Append(client);
        client->Rearm(0);
    } else {
        delete client;
    }
}

void http_Daemon::Dispatcher::Close()
{
    for (SocketData *socket: sockets) {
        delete socket;
    }
    for (http_IO *client: free_clients) {
        delete client;
    }

    sockets.Clear();
    free_sockets.Clear();
    free_clients.Clear();
}

void http_IO::Send(int status, CompressionType encoding, int64_t len, FunctionRef<bool(int, StreamWriter *)> func)
{
    RG_ASSERT(!response.sent);

    if (request.headers_only) {
        func = [](int, StreamWriter *) { return true; };
    }

    RG_DEFER { response.sent = true; };

    const auto write = [this](Span<const uint8_t> buf) { return WriteDirect(buf); };
    StreamWriter writer(write, "<http>");

    LocalArray<char, 32768> intro;

    const char *protocol = (request.version == 11) ? "HTTP/1.1" : "HTTP/1.0";
    const char *details = http_ErrorMessages.FindValue(status, "Unknown");

    if (request.keepalive) {
        intro.len += Fmt(intro.TakeAvailable(), "%1 %2 %3\r\n"
                                                "Connection: keep-alive\r\n",
                         protocol, status, details).len;
    } else {
        intro.len += Fmt(intro.TakeAvailable(), "%1 %2 %3\r\n"
                                                "Connection: close\r\n",
                         protocol, status, details).len;
    }

    switch (encoding) {
        case CompressionType::None: {} break;
        case CompressionType::Zlib: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: deflate\r\n").len; } break;
        case CompressionType::Gzip: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: gzip\r\n").len; } break;
        case CompressionType::Brotli: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: br\r\n").len; } break;
        case CompressionType::LZ4: { RG_UNREACHABLE(); } break;
        case CompressionType::Zstd: { intro.len += Fmt(intro.TakeAvailable(), "Content-Encoding: zstd\r\n").len; } break;
    }

    for (const http_KeyValue &header: response.headers) {
        intro.len += Fmt(intro.TakeAvailable(), "%1: %2\r\n", header.key, header.value).len;
    }

    if (len >= 0) {
        intro.len += Fmt(intro.TakeAvailable(), "Content-Length: %1\r\n\r\n", len).len;

        if (!intro.Available()) [[unlikely]] {
            LogError("Excessive length for response headers");

            request.keepalive = false;
            return;
        }

        writer.Write(intro);

        if (encoding != CompressionType::None) {
            writer.Close();
            writer.Open(write, "<http>", encoding);
        }

        request.keepalive &= func(fd, &writer);
    } else {
        intro.len += Fmt(intro.TakeAvailable(), "Transfer-Encoding: chunked\r\n\r\n").len;

        if (!intro.Available()) [[unlikely]] {
            LogError("Excessive length for response headers");

            request.keepalive = false;
            return;
        }

        writer.Write(intro);

        const auto chunk = [this](Span<const uint8_t> buf) { return WriteChunked(buf); };
        StreamWriter chunker(chunk, "<http>", encoding);

        if (func(-1, &chunker)) {
            request.keepalive &= chunker.Close();
            writer.Write("0\r\n\r\n");
        } else {
            request.keepalive = false;
        }
    }

    request.keepalive &= writer.Close();
}

bool http_IO::SendFile(int status, const char *filename, const char *mimetype)
{
    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return false;
    RG_DEFER { CloseDescriptor(fd); };

    FileInfo file_info;
    if (StatFile(fd, filename, &file_info) != StatResult::Success)
        return false;
    if (file_info.type != FileType::File) {
        LogError("Cannot serve non-regular file '%1'", filename);
        return false;
    }
    int64_t len = file_info.size;

    if (mimetype) {
        AddHeader("Content-Type", mimetype);
    }

    Send(status, len, [&](int sock, StreamWriter *) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);

        if (len) {
            for (;;) {
                DWORD send = (DWORD)std::min(len, (Size)UINT32_MAX);
                BOOL success = TransmitFile((SOCKET)sock, h, send, 0, nullptr, nullptr, 0);

                if (!success) {
                    LogError("Failed to send file: %1", strerror(TranslateWinSockError()));
                    return false;
                }

                len -= (Size)send;
                if (!len)
                    break;

                if (!SetFilePointerEx(h, { .QuadPart = send }, nullptr, FILE_CURRENT)) {
                    LogError("Failed to send file: %1", GetWin32ErrorString());
                    return false;
                }
            }
        }

        return true;
    });

    return true;
}

http_IO::PrepareStatus http_IO::Prepare(int64_t now)
{
    if (ready)
        return PrepareStatus::Busy;
    if (fd < 0)
        return PrepareStatus::Unused;

    bool complete = false;

    // Gather request line and headers
    while (incoming.buf.len - incoming.pos >= 4) {
        uint8_t *next = (uint8_t *)memchr(incoming.buf.ptr + incoming.pos, '\r', incoming.buf.len - incoming.pos);
        incoming.pos = next ? next - incoming.buf.ptr : incoming.buf.len;

        if (incoming.pos >= daemon->max_request_size) [[unlikely]] {
            LogError("Excessive request size");
            SendError(413);
            return PrepareStatus::Close;
        }

        const char *end = (const char *)incoming.buf.ptr + incoming.pos;

        if (end[0] == '\r' && end[1] == '\n' && end[2] == '\r' && end[3] == '\n') {
            incoming.intro = incoming.buf.As<char>().Take(0, incoming.pos);
            incoming.extra = MakeSpan(incoming.buf.ptr + incoming.pos + 4, incoming.buf.len - incoming.pos - 4);

            complete = true;
            break;
        } else if (end[0] == '\n' && end[1] == '\n') {
            incoming.intro = incoming.buf.As<char>().Take(0, incoming.pos);
            incoming.extra = MakeSpan(incoming.buf.ptr + incoming.pos + 2, incoming.buf.len - incoming.pos - 2);

            complete = true;
            break;
        }

        incoming.pos++;
    }

    if (!complete)
        return PrepareStatus::Incoming;
    if (!ParseRequest(incoming.intro))
        return PrepareStatus::Close;

    ready = true;
    return PrepareStatus::Ready;
}

bool http_IO::WriteDirect(Span<const uint8_t> data)
{
    while (data.len) {
        int len = (int)std::min(data.len, (Size)INT_MAX);
        Size sent = send((SOCKET)fd, (const char *)data.ptr, len, 0);

        if (sent < 0) {
            if (errno == EINTR)
                continue;

            errno = TranslateWinSockError();

            if (errno != EPIPE && errno != ECONNRESET) {
                LogError("Failed to send to client: %1", strerror(errno));
            }
            return false;
        }

        data.ptr += sent;
        data.len -= sent;
    }

    return true;
}

bool http_IO::WriteChunked(Span<const uint8_t> data)
{
    while (data.len) {
        LocalArray<uint8_t, 16384> buf;

        Size copy_len = std::min(RG_SIZE(buf.data) - 8, data.len);

        buf.len = 8 + copy_len;
        Fmt(buf.As<char>(), "%1\r\n", FmtHex(copy_len).Pad0(-4));
        MemCpy(buf.data + 6, data.ptr, copy_len);
        buf.data[6 + copy_len + 0] = '\r';
        buf.data[6 + copy_len + 1] = '\n';

        Span<const uint8_t> remain = buf;

        do {
            int len = (int)std::min(remain.len, (Size)INT_MAX);
            Size sent = send((SOCKET)fd, (const char *)remain.ptr, len, 0);

            if (sent < 0) {
                if (errno == EINTR)
                    continue;

                errno = TranslateWinSockError();

                if (errno != EPIPE && errno != ECONNRESET) {
                    LogError("Failed to send to client: %1", strerror(errno));
                }
                return false;
            }

            remain.ptr += sent;
            remain.len -= sent;
         } while (remain.len);

        data.ptr += copy_len;
        data.len -= copy_len;
    }

    return true;
}

}

#endif
