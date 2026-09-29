// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "event.hh"

namespace K {

void http_EventCounter::Prune()
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    int64_t clock = GetMonotonicClock();

    Size expired = 0;
    for (const EventData &event: events) {
        if (event.info.until > clock)
            break;

        EventData **ptr = map.Find(event.key);
        if (*ptr == &event) {
            map.Remove(ptr);
        }
        expired++;
    }
    events.RemoveFirst(expired);

    events.Trim();
    map.Trim();
}

const http_EventInfo *http_EventCounter::Register(const char *partition, const char *what, int64_t time)
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    EventKey key = { partition, what };
    EventData *event = map.FindValue(key, nullptr);

    if (!event || event->info.until < GetMonotonicClock()) {
        Allocator *alloc;
        event = events.AppendDefault(&alloc);

        event->key.partition = DuplicateString(key.partition, alloc).ptr;
        event->key.what = DuplicateString(key.what, alloc).ptr;
        event->info.until = GetMonotonicClock() + timeout;

        map.Set(event);
    }

    event->info.count++;
    event->info.prev_time = event->info.time;
    event->info.time = time;

    return &event->info;
}

int http_EventCounter::Count(const char *partition, const char *what)
{
    std::shared_lock<std::shared_mutex> lock_shr(mutex);

    EventKey key = { partition, what };
    const EventData *event = map.FindValue(key, nullptr);

    // We don't need to use precise timing, and a ban can last a bit
    // more than BanTime (until pruning clears the ban).
    return event ? event->info.count : 0;
}

}
