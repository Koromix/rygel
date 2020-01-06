// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "events.hh"
#include "goupile.hh"

namespace RG {

struct PushContext {
    PushContext *prev;
    PushContext *next;

    MHD_Connection *conn;

    std::mutex mutex;
    bool suspended = false;
    unsigned int events = 0;
};

static bool run = true;

static std::mutex mutex;
static PushContext root = {&root, &root};

static ssize_t SendClientEvents(void *cls, uint64_t, char *buf, size_t max)
{
    PushContext *ctx = (PushContext *)cls;

    if (run) {
        std::lock_guard<std::mutex> lock(ctx->mutex);

        RG_ASSERT(ctx->events);
        int ctz = CountTrailingZeros(ctx->events);
        ctx->events &= ~(1u << ctz);

        if (!ctx->events && !ctx->suspended) {
            MHD_suspend_connection(ctx->conn);
            ctx->suspended = true;
        }

        // XXX: This may result in truncation when max is very low
        return Fmt(MakeSpan(buf, max), "event: %1\ndata: {}\n\n", EventTypeNames[ctz]).len;
    } else {
        return MHD_CONTENT_READER_END_OF_STREAM;
    }
}

static void UnregisterEventConnection(PushContext *ctx)
{
    std::lock_guard lock(mutex);

    ctx->prev->next = ctx->next;
    ctx->next->prev = ctx->prev;
}

void CloseAllEventConnections()
{
    std::unique_lock<std::mutex> lock(mutex);

    run = false;

    // Wake up all SSE connections
    PushContext *ctx = root.next;
    while (ctx != &root) {
        std::lock_guard<std::mutex> lock_ctx(ctx->mutex);

        if (ctx->suspended) {
            MHD_resume_connection(ctx->conn);
            ctx->suspended = false;
        }

        ctx = ctx->next;
    }

    // Wait until all SSE connections are over
    while (root.prev != root.next) {
        lock.unlock();
        WaitForDelay(20);
        lock.lock();
    }
}

void HandleEvents(const http_RequestInfo &request, http_IO *io)
{
    PushContext *ctx = (PushContext *)Allocator::Allocate(&io->allocator, RG_SIZE(*ctx));
    new (ctx) PushContext();
    ctx->conn = request.conn;

    // Issuing keepalive is better for Firefox. For a start, the open event gets triggered.
    ctx->events = 1u << (int)EventType::KeepAlive;

    // Register SSE connection
    {
        std::lock_guard lock(mutex);

        if (!run) {
            LogError("Server is shutting down");
            return;
        }

        root.prev->next = ctx;
        ctx->next = &root;
        ctx->prev = root.prev;
        root.prev = ctx;
    }

    MHD_Response *response =
        MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024, SendClientEvents, ctx,
                                          [](void *cls) { UnregisterEventConnection((PushContext *)cls); });

    io->AttachResponse(200, response);
    io->AddHeader("Content-Type", "text/event-stream");
    io->AddHeader("Cache-Control", "no-cache");
    io->AddHeader("Connection", "keep-alive");
}

static void PushEvents(unsigned int events)
{
    std::lock_guard<std::mutex> lock(mutex);

    PushContext *ctx = root.next;
    while (ctx != &root) {
        std::lock_guard<std::mutex> lock_ctx(ctx->mutex);

        ctx->events |= events;
        if (ctx->suspended) {
            MHD_resume_connection(ctx->conn);
            ctx->suspended = false;
        }

        ctx = ctx->next;
    }
}

void PushEvent(EventType type)
{
    PushEvents(1u << (int)type);
}

}
