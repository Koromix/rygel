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
#include "rekkord.hh"
#include "src/core/request/curl.hh"
#include "src/core/wrap/json.hh"

#include <poll.h>

namespace K {

struct ItemData {
    const char *channel;
    int clock;
    int days;
    HeapArray<const char *> paths;

    int64_t timestamp;
    bool success;
};

static std::mutex plan_mutex;
static HeapArray<ItemData> plan_items;
static BlockAllocator plan_alloc;

static void LinkHeaders(Span<curl_slist> headers)
{
    if (!headers.len)
        return;

    for (Size i = 0; i < headers.len - 1; i++) {
        headers[i].next = &headers[i + 1];
    }
    headers[headers.len - 1].next = nullptr;
}

static bool FetchPlan(Allocator *alloc, HeapArray<ItemData> *out_items)
{
    K_DEFER_NC(err_guard, len = out_items->len) { out_items->RemoveFrom(len); };

    Span<const char> api = rk_config.agent_url;
    const char *key = rk_config.api_key;

    HeapArray<char> body;
    {
        CURL *curl = curl_Init();
        if (!curl)
            return false;
        K_DEFER { curl_easy_cleanup(curl); };

        const char *url = Fmt(alloc, "%1/api/plan/fetch", TrimStrRight(api, '/')).ptr;

        curl_slist headers[] = {
            { Fmt(alloc, "X-Api-Key: %1", key).ptr, nullptr }
        };
        LinkHeaders(headers);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, &headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
            HeapArray<char> *body = (HeapArray<char> *)udata;

            Span<const char> buf = MakeSpan(ptr, (Size)nmemb);
            body->Append(buf);

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

        int status = curl_Perform(curl, "fetch");

        if (status != 200) {
            if (status >= 0) {
                LogError("Failed to fetch plan with status %1", status);
            }
            return false;
        }
    }

    // Parse plan
    {
        StreamReader st(body.As<uint8_t>(), "<plan>");
        json_Parser json(&st, alloc);

        for (json.ParseArray(); json.InArray(); ) {
            ItemData item = {};

            for (json.ParseObject(); json.InObject(); ) {
                Span<const char> key = json.ParseKey();

                if (key == "id") {
                    json.Skip();
                } else if (key == "channel") {
                    json.ParseString(&item.channel);
                } else if (key == "clock") {
                    json.ParseInt(&item.clock);
                } else if (key == "days") {
                    json.ParseInt(&item.days);
                } else if (key == "timestamp") {
                    json.SkipNull() || json.ParseInt(&item.timestamp);
                } else if (key == "success") {
                    json.ParseBool(&item.success);
                } else if (key == "paths") {
                    for (json.ParseArray(); json.InArray(); ) {
                        const char *path = json.ParseString().ptr;
                        item.paths.Append(path);
                    }
                } else {
                    json.UnexpectedKey(key);
                    return false;
                }
            }

            out_items->Append(item);
        }
        if (!json.IsValid())
            return false;
    }

    err_guard.Disable();
    return true;
}

static bool ShouldRun(const ItemData &item)
{
    K_ASSERT(item.days & 0b1111111);

    int64_t now = GetUnixTime();

    if (now - item.timestamp >= 7 * 86400000)
        return true;
    if (!item.success)
        return true;

    TimeSpec then = DecomposeTimeUTC(item.timestamp);
    TimeSpec spec = DecomposeTimeUTC(now);
    LocalDate date = LocalDate(then.year, then.month, then.day);
    LocalDate today = LocalDate(spec.year, spec.month, spec.day);

    if (date < today) {
        date++;
        while (date < today) {
            if (item.days & (1 << date.GetWeekDay()))
                return true;
            date++;
        }
    }

    if (item.days & (1 << date.GetWeekDay())) {
        int hhmm1 = then.hour * 100 + then.min;
        int hhmm2 = spec.hour * 100 + spec.min;

        if (hhmm1 < item.clock && hhmm2 >= item.clock)
            return true;
    }

    return false;
}

static bool SendReport(Span<const char> json)
{
    BlockAllocator temp_alloc;

    Span<const char> api = rk_config.agent_url;
    const char *key = rk_config.api_key;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    K_DEFER { curl_easy_cleanup(curl); };

    const char *url = Fmt(&temp_alloc, "%1/api/plan/report", TrimStrRight(api, '/')).ptr;

    curl_slist headers[] = {
        { (char *)"Content-Type: application/json", nullptr },
        { Fmt(&temp_alloc, "X-Api-Key: %1", key).ptr, nullptr }
    };
    LinkHeaders(headers);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, &headers);

    Span<const char> remain = json;

    curl_easy_setopt(curl, CURLOPT_POST, 1L); // POST
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *udata) {
        Span<const char> *remain = (Span<const char> *)udata;
        Size give = std::min((Size)(size * nmemb), remain->len);

        MemCpy(ptr, remain->ptr, give);
        remain->ptr += (Size)give;
        remain->len -= (Size)give;

        return (size_t)give;
    });
    curl_easy_setopt(curl, CURLOPT_READDATA, &remain);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)remain.len);

    int status = curl_Perform(curl, "report");

    if (status != 200) {
        if (status >= 0) {
            LogError("Failed to send report with status %1", status);
        }
        return false;
    }

    return true;
}

static bool UpdatePlan()
{
    LogInfo("Fetching backup plan...");

    HeapArray<ItemData> items;
    BlockAllocator str_alloc;

    if (!FetchPlan(&str_alloc, &items))
        return false;

    std::lock_guard<std::mutex> lock(plan_mutex);

    std::swap(items, plan_items);
    std::swap(str_alloc, plan_alloc);

    InterruptWait();

    return true;
}

static bool ReportSuccess(const char *channel, const rk_SaveInfo &info)
{
    HeapArray<char> body;

    char oid[128];
    Fmt(oid, "%1", info.oid);

    // Format JSON
    {
        StreamWriter st(&body, "<report>");
        json_Writer json(&st);

        json.StartObject();
        json.Key("channel"); json.String(channel);
        json.Key("timestamp"); json.Int64(info.time);
        json.Key("oid"); json.String(oid);
        json.Key("size"); json.Int64(info.size);
        json.Key("stored"); json.Int64(info.stored);
        json.Key("added"); json.Int64(info.added);
        json.EndObject();
    }

    return SendReport(body);
}

static bool ReportError(const char *channel, int64_t time, const char *message)
{
    HeapArray<char> body;

    // Format JSON
    {
        StreamWriter st(&body, "<report>");
        json_Writer json(&st);

        json.StartObject();
        json.Key("channel"); json.String(channel);
        json.Key("timestamp"); json.Int64(time);
        json.Key("error"); json.String(message);
        json.EndObject();
    }

    return SendReport(body);
}

static bool RunSnapshot(const char *channel, Span<const char *const> paths, rk_SaveInfo *out_info)
{
    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rk_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rk_config, true);
    if (!repo)
        return false;

    return rk_Save(repo.get(), channel, paths, {}, out_info);
}

static bool RunPlan()
{
    bool busy = false;

    for (ItemData &item: plan_items) {
        bool run = ShouldRun(item);

        if (run) {
            LogInfo("Running snapshot for '%1'", item.channel);

            const char *last_err = "Unknown error";
            PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
                if (level == LogLevel::Error) {
                    last_err = DuplicateString(msg, &plan_alloc).ptr;
                }

                func(level, ctx, msg);
            });
            K_DEFER { PopLogFilter(); };

            rk_SaveInfo info = {};
            bool success = RunSnapshot(item.channel, item.paths, &info);

            if (success) {
                ReportSuccess(item.channel, info);

                item.timestamp = info.time;
                item.success = true;
            } else {
                int64_t now = GetUnixTime();
                ReportError(item.channel, now, last_err);

                item.timestamp = now;
                item.success = false;
            }
        }

        busy |= run;
    }

    if (busy) {
        InterruptWait();
    } else {
        LogInfo("Nothing to do");
    }

    return true;
}

#if !defined(_WIN32)

static int OpenControl(const char *filename)
{
    int fd = CreateSocket(SocketType::Unix, SOCK_STREAM);
    if (fd < 0)
        return -1;
    K_DEFER_N(err_guard) { close(fd); };

    if (!BindUnixSocket(fd, filename))
        return -1;
    if (listen(fd, 4) < 0) {
        LogError("listen() failed: %1", strerror(errno));
        return -1;
    }

    err_guard.Disable();
    return fd;
}

static int AcceptClient(int fd)
{
#if defined(SOCK_CLOEXEC)
    int sock = accept4(fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
    int sock = accept(fd, nullptr, nullptr);
#endif

    if (sock < 0) {
        LogError("Failed to accept client: %1", strerror(errno));
        return -1;
    }

#if !defined(SOCK_CLOEXEC)
    fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
#if !defined(MSG_DONTWAIT)
    SetDescriptorNonBlock(sock, true);
#endif

    return sock;
}

static void CloseClient(int fd)
{
    CloseDescriptor(fd);
}

static Size DoForClients(Span<WaitSource> sources, FunctionRef<bool(Size idx, int fd)> func)
{
    Size j = 1;
    for (Size i = 1; i < sources.len; i++) {
        sources[j] = sources[i];

        if (!func(i, sources[i].fd)) {
            close(sources[i].fd);
            continue;
        }

        j++;
    }
    return j;
}

static void OpenClientRead(int sock, StreamReader *out_st)
{
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

    bool success = out_st->Open(read, "<client>");
    K_ASSERT(success);
}

static void OpenClientWrite(int sock, StreamWriter *out_st)
{
    const auto write = [sock](Span<const uint8_t> buf) {
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

    bool success = out_st->Open(write, "<client>");
    K_ASSERT(success);
}

#endif

// Call with plan_mutex locked
static bool SendInfo(StreamWriter *writer)
{
    json_Writer json(writer);

    json.StartObject();

    json.Key("items"); json.StartArray();
    for (const ItemData &item: plan_items) {
        json.StartObject();

        json.Key("channel"); json.String(item.channel);
        json.Key("clock"); json.Int(item.clock);
        json.Key("days"); json.Int(item.days);

        json.Key("timestamp"); json.Int64(item.timestamp);
        json.Key("success"); json.Bool(item.success);

        json.EndObject();
    }
    json.EndArray();

    json.EndObject();

    if (!writer->Write('\n'))
        return false;

    return true;
}

static bool ReadClient(StreamReader *reader)
{
    BlockAllocator temp_alloc;

    json_Parser json(reader, &temp_alloc);

    // Only empty objects for now
    for (json.ParseObject(); json.InObject(); ) {
        Span<const char> key = json.ParseKey();

        json.UnexpectedKey(key);
        return false;
    }
    if (!json.IsValid())
        return false;

    return true;
}

int RunAgent(Span<const char *> arguments)
{
    // Options
    const char *control_path = "/run/rekkord.sock";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 agent [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
        PrintLn(st, T(R"(
Agent options:

    %!..+-S, --socket_file socket%!0       Change control socket
                                   %!D..(default: %1)%!0)"),
                control_path);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-S", "--socket_file", OptionType::Value)) {
                control_path = opt.current_value;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    // Validate configuration
    {
        unsigned int flags = (int)rk_ConfigFlag::RequireAuth | (int)rk_ConfigFlag::RequireAgent;

        if (!rk_config.Complete(flags))
            return 1;
        if (!rk_config.Validate(flags))
            return 1;
    }

    // Check repository connection
    {
        std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rk_config);
        std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rk_config, true);
        if (!repo)
            return 1;

        LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
        if (!repo->HasMode(rk_AccessMode::Read)) {
            LogError("Cannot restore data with %1 key", repo->GetRole());
            return 1;
        }
        LogInfo();
    }

    // Open control socket or pipe
    auto control = OpenControl(control_path);
    if (control < 0)
        return 1;
    K_DEFER { CloseDescriptor(control); };

    // From here on, don't quit abruptly
    WaitEvents(0);

    // Make sure we can fetch plan (valid URL and credentials)
    if (!UpdatePlan())
        return 1;

    // Run two event loops: one for plan fetch and execution, one for clients.
    // Makes things easier, with a simple mutex for synchronization.
    std::thread thread([&]() {
        for (;;) {
            RunPlan();

            WaitResult ret = WaitEvents(rk_config.agent_period);

            if (ret == WaitResult::Exit || ret == WaitResult::Interrupt)
                break;
            K_ASSERT(ret != WaitResult::Message);

            UpdatePlan();
        }
    });
    K_DEFER { thread.join(); };

    // Handle clients (such as RekkordTray)
    int status = 0;
    {
        LocalArray<WaitSource, 32> sources;

        sources.Append({ control, -1 });

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
            }

            if (ready & 1) {
                auto desc = AcceptClient(control);

                if (desc >= 0) {
                    if (sources.Available()) {
                        sources.Append({ desc, -1 });

                        StreamWriter writer;
                        OpenClientWrite(desc, &writer);

                        std::unique_lock<std::mutex> lock(plan_mutex);
                        SendInfo(&writer);
                    } else {
                        LogError("Cannot handle new client (too many)");
                        CloseClient(desc);
                    }
                }
            }

            sources.len = DoForClients(sources, [&](Size idx, auto desc) {
                if (!(ready & (1u << idx)))
                    return true;

                StreamReader reader;
                OpenClientRead(desc, &reader);

                return ReadClient(&reader);
            });

            if (ret == WaitResult::Message) {
                sources.len = DoForClients(sources, [&](Size, auto desc) {
                    StreamWriter writer;
                    OpenClientWrite(desc, &writer);

                    return SendInfo(&writer);
                });
            }
        }
    }

    return status;
}

}
