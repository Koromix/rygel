// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/wrap/json.hh"

namespace K {

static const int MaxTowerSources = 10;

class TowerServer {
#if defined(_WIN32)
    char name[512] = {};
    LocalArray<struct OverlappedPipe *, MaxTowerSources> pipes;
#else
    int fd = -1;
#endif

    LocalArray<WaitSource, MaxTowerSources> sources;

    std::function<bool(StreamReader *, StreamWriter *)> handle_func;

public:
    TowerServer() {}
    ~TowerServer() { Stop(); }

    bool Bind(const char *path);
    void Start(std::function<bool(StreamReader *, StreamWriter *)> func);
    void Stop();

    Span<const WaitSource> GetWaitSources() const { return sources; }
    bool Process(uint64_t ready = UINT64_MAX);

    void Send(FunctionRef<void(StreamWriter *)> func);
    void Send(Span<const uint8_t> buf) { Send([&](StreamWriter *writer) { writer->Write(buf); }); }
    void Send(Span<const char> buf) { Send([&](StreamWriter *writer) { writer->Write(buf); }); }

private:
#if defined(_WIN32)
    void RunClients(FunctionRef<bool(Size, struct OverlappedPipe *)> func);
#else
    void RunClients(FunctionRef<bool(Size, int)> func);
#endif
};

class TowerClient {
#if defined(_WIN32)
    struct OverlappedPipe *pipe = nullptr;
#else
    int sock = -1;
#endif

    std::function<void(StreamReader *)> handle_func;

    WaitSource src = {};

public:
    ~TowerClient() { Stop(); }

    bool Connect(const char *path);
    void Start(std::function<void(StreamReader *)> func);
    void Stop();

    WaitSource GetWaitSource() const { return src; }
    bool Process();

    bool Send(FunctionRef<void(StreamWriter *)> func);
    bool Send(Span<const uint8_t> buf) { return Send([&](StreamWriter *writer) { writer->Write(buf); }); }
    bool Send(Span<const char> buf) { return Send([&](StreamWriter *writer) { writer->Write(buf); }); }
};

enum class ControlScope {
    System,
    User
};

const char *GetControlSocketPath(ControlScope scope, const char *name, Allocator *alloc);

}
