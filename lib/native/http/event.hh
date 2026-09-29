// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

struct http_EventInfo {
    int64_t until; // Monotonic

    int count;
    int64_t prev_time; // Unix time
    int64_t time; // Unix time
};

class http_EventCounter {
    K_DELETE_COPY(http_EventCounter)

    struct EventKey {
        const char *partition;
        const char *what;

        bool operator==(const EventKey &other) const { return TestStr(partition, other.partition) && TestStr(what, other.what); }
        bool operator!=(const EventKey &other) const { return !(*this == other); }

        uint64_t Hash() const
        {
            uint64_t hash = HashTraits<const char *>::Hash(partition) ^
                            HashTraits<const char *>::Hash(what);
            return hash;
        }
    };

    struct EventData {
        EventKey key;
        http_EventInfo info;

        K_HASHTABLE_HANDLER(EventData, key);
    };

    int timeout;

    std::shared_mutex mutex;
    BucketList<EventData> events;
    HashTable<EventKey, EventData *> map;

public:
    http_EventCounter(int timeout) : timeout(timeout) {}

    void Prune();

    const http_EventInfo *Register(const char *partition, const char *what, int64_t time);
    int Count(const char *partition, const char *what);

    template<typename T>
    const http_EventInfo *Register(const char *partition, T what, int64_t time)
    {
        char buf[256];
        Fmt(buf, "%1", what);

        return Register(partition, buf, time);
    }
    template<typename T>
    int Count(const char *partition, T what)
    {
        char buf[256];
        Fmt(buf, "%1", what);

        return Count(partition, buf);
    }
};

}
