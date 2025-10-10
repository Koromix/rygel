// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

#pragma once

#include "src/core/base/base.hh"
#include "src/core/wrap/json.hh"

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
