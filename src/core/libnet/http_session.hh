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

#pragma once

#include "src/core/libcc/libcc.hh"
#include "http.hh"

namespace RG {

template <typename T>
class http_SessionManager {
    RG_DELETE_COPY(http_SessionManager)

    static const int64_t MaxSessionDelay = 1440 * 60000;
    static const int64_t MaxKeyDelay = 15 * 60000;
    static const int64_t MaxLockDelay = 120 * 60000;
    static const int64_t RegenerateDelay = 5 * 60000;

    struct SessionHandle {
        char session_key[65];
        char session_rnd[33];

        int64_t login_time;
        int64_t register_time;
        int64_t lock_time;

        RetainPtr<T> udata;

        RG_HASHTABLE_HANDLER_T(SessionHandle, const char *, session_key);
    };

    const char *cookie_path = "/";

    std::shared_mutex mutex;
    BucketArray<SessionHandle> sessions;
    HashTable<const char *, SessionHandle *> sessions_map;

public:
    http_SessionManager() = default;

    void SetCookiePath(const char *new_path) { cookie_path = new_path; }

    void Open(const http_RequestInfo &, http_IO *io, RetainPtr<T> udata)
    {
        std::lock_guard<std::shared_mutex> lock_excl(mutex);

        SessionHandle *handle = CreateHandle();
        if (!handle)
            return;
        int64_t now = GetMonotonicTime();

        handle->login_time = now;
        handle->register_time = now;
        handle->lock_time = now;
        handle->udata = udata;

        // Set session cookies
        io->AddCookieHeader(cookie_path, "session_key", handle->session_key, true);
        io->AddCookieHeader(cookie_path, "session_rnd", handle->session_rnd, false);
    }

    void Close(const http_RequestInfo &request, http_IO *io)
    {
        std::lock_guard<std::shared_mutex> lock_excl(mutex);

        // We don't care about those but for performance reasons FindHandle()
        // always writes those.
        bool mismatch;
        bool locked;
        SessionHandle **ptr = FindHandle(request, &mismatch, &locked);

        sessions_map.Remove(ptr);
        DeleteSessionCookies(io);
    }

    RetainPtr<T> Find(const http_RequestInfo &request, http_IO *io)
    {
        std::shared_lock<std::shared_mutex> lock_shr(mutex);

        bool mismatch = false;
        bool locked = false;
        SessionHandle **ptr = FindHandle(request, &mismatch, &locked);

        if (ptr) {
            SessionHandle *handle = *ptr;
            RetainPtr<T> udata = handle->udata;
            int64_t now = GetMonotonicTime();

            // Regenerate session if needed
            if (now - handle->register_time >= RegenerateDelay) {
                static_assert(RG_SIZE(handle->session_rnd) == 33);

                char session_rnd[33];
                CopyString(handle->session_rnd, session_rnd);
                int64_t login_time = handle->login_time;
                int64_t lock_time = handle->lock_time;

                lock_shr.unlock();
                std::lock_guard<std::shared_mutex> lock_excl(mutex);

                handle = CreateHandle(locked ? session_rnd : nullptr);
                if (!handle) {
                    DeleteSessionCookies(io);
                    return nullptr;
                }

                handle->login_time = login_time;
                handle->register_time = now;
                handle->lock_time = locked ? lock_time : now;
                handle->udata = udata;

                // Set session cookies
                io->AddCookieHeader(cookie_path, "session_key", handle->session_key, true);
                if (!locked) {
                    io->AddCookieHeader(cookie_path, "session_rnd", handle->session_rnd, false);
                }
            }

            if (!locked) {
                return udata;
            } else {
                return nullptr;
            }
        } else if (mismatch) {
            DeleteSessionCookies(io);
            return nullptr;
        } else {
            return nullptr;
        }
    }

    void Prune()
    {
        std::lock_guard<std::shared_mutex> lock_excl(mutex);

        int64_t now = GetMonotonicTime();

        Size expired = 0;
        for (const SessionHandle &handle: sessions) {
            if (now - handle.register_time < MaxKeyDelay)
                break;

            sessions_map.Remove(handle.session_key);
            expired++;
        }

        sessions.RemoveFirst(expired);

        sessions.Trim();
        sessions_map.Trim();
    }

    void ApplyAll(FunctionRef<void(T *udata)> func)
    {
        std::lock_guard<std::shared_mutex> lock_excl(mutex);

        for (const SessionHandle &handle: sessions) {
            func(handle.udata.GetRaw());
        }
    }

private:
    SessionHandle *CreateHandle(const char *session_rnd = nullptr)
    {
        SessionHandle *handle = sessions.AppendDefault();

        // Register handle with unique key
        for (;;) {
            static_assert(RG_SIZE(SessionHandle::session_key) == 65);

            {
                uint64_t buf[4];
                FillRandomSafe(buf, RG_SIZE(buf));
                Fmt(handle->session_key, "%1%2%3%4",
                    FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16),
                    FmtHex(buf[2]).Pad0(-16), FmtHex(buf[3]).Pad0(-16));
            }

            bool inserted;
            sessions_map.TrySet(handle, &inserted);

            if (inserted) [[likely]]
                break;
        }

        // Reuse or create public randomized key (for use in session-specific URLs)
        if (session_rnd) {
            RG_ASSERT(strlen(session_rnd) + 1 == RG_SIZE(handle->session_rnd));
            CopyString(session_rnd, handle->session_rnd);
        } else {
            static_assert(RG_SIZE(handle->session_rnd) == 33);

            uint64_t buf[2];
            FillRandomSafe(&buf, RG_SIZE(buf));
            Fmt(handle->session_rnd, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
        }

        return handle;
    }

    SessionHandle **FindHandle(const http_RequestInfo &request, bool *out_mismatch, bool *out_locked)
    {
        int64_t now = GetMonotonicTime();

        const char *session_key = request.GetCookieValue("session_key");
        const char *session_rnd = request.GetCookieValue("session_rnd");
        if (!session_key) {
            *out_mismatch = false;
            return nullptr;
        }

        SessionHandle **ptr = sessions_map.Find(session_key);
        if (!ptr) {
            *out_mismatch = true;
            return nullptr;
        }

        // Until 2020-08-20 there was an IP check below, but it caused problems with mobile
        // connectivity and with dual-stack browsers. For example, on occasion, I would get
        // disconnected during localhost tests because login used IPv4 and a subsequent request
        // used IPv6, or vice versa.
        SessionHandle *handle = *ptr;
        if (now - handle->login_time >= MaxSessionDelay ||
                now - handle->register_time >= MaxKeyDelay ||
                now - handle->lock_time >= MaxLockDelay ||
                (session_rnd && !TestStr(handle->session_rnd, session_rnd))) {
            *out_mismatch = true;
            return nullptr;
        }

        *out_mismatch = false;
        *out_locked = !session_rnd;
        return ptr;
    }

    void DeleteSessionCookies(http_IO *io)
    {
        io->AddCookieHeader(cookie_path, "session_key", nullptr, true);
        io->AddCookieHeader(cookie_path, "session_rnd", nullptr, false);
    }
};

}
