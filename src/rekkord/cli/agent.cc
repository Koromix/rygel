// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/base/tower.hh"
#include "rekkord.hh"
#include "link.hh"
#include "lib/native/request/curl.hh"
#include "lib/native/wrap/json.hh"

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

static TowerServer server;

static bool FetchPlan(Allocator *alloc, HeapArray<ItemData> *out_items)
{
    K_DEFER_NC(err_guard, len = out_items->len) { out_items->RemoveFrom(len); };

    HeapArray<char> body;
    {
        CURL *curl = curl_Init();
        if (!curl)
            return false;
        K_DEFER { curl_easy_cleanup(curl); };

        const char *url = Fmt(alloc, "%1/api/plan/fetch", TrimStrRight(rk_config.link_url, '/')).ptr;

        curl_slist headers[] = {
            { Fmt(alloc, "X-Api-Key: %1", rk_config.link_key).ptr, nullptr }
        };

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, &headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
            HeapArray<char> *body = (HeapArray<char> *)udata;

            Span<const char> buf = MakeSpan(ptr, (Size)nmemb);
            body->Append(buf);

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

        int status = curl_Perform(curl, "fetch backup plan");

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

static bool UpdatePlan(bool post)
{
    LogInfo("Fetching backup plan...");

    HeapArray<ItemData> items;
    BlockAllocator str_alloc;

    if (!FetchPlan(&str_alloc, &items))
        return false;

    std::lock_guard<std::mutex> lock(plan_mutex);

    std::swap(items, plan_items);
    std::swap(str_alloc, plan_alloc);

    if (post) {
        PostWaitMessage();
    }

    return true;
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
                ReportSnapshot(rk_config.link_url, rk_config.link_key, rk_config.url, item.channel, info);

                item.timestamp = info.time;
                item.success = true;
            } else {
                int64_t now = GetUnixTime();
                ReportError(rk_config.link_url, rk_config.link_key, rk_config.url, item.channel, now, last_err);

                item.timestamp = now;
                item.success = false;
            }
        }

        busy |= run;
    }

    if (busy) {
        PostWaitMessage();
    } else {
        LogInfo("Nothing to do");
    }

    return true;
}

// Call with plan_mutex locked
static void SendInfo(StreamWriter *writer)
{
    json_CompactWriter json(writer);

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

    writer->Write('\n');
}

static bool HandleClientData(StreamReader *reader, StreamWriter *writer)
{
    BlockAllocator temp_alloc;

    json_Parser json(reader, &temp_alloc);
    bool refresh = false;

    for (json.ParseObject(); json.InObject(); ) {
        Span<const char> key = json.ParseKey();

        if (key == "refresh") {
            json.ParseBool(&refresh);
        } else {
            json.UnexpectedKey(key);
            return false;
        }
    }
    if (!json.IsValid())
        return false;

    if (refresh) {
        std::unique_lock<std::mutex> lock(plan_mutex);
        SendInfo(writer);
    }

    return true;
}

int RunAgent(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *socket_filename = GetControlSocketPath(ControlScope::System, "rekkord", &temp_alloc);

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 agent [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
        PrintLn(st, T(R"(
Agent options:

    %!..+-S, --socket_file socket%!0       Change control socket
                                   %!D..(default: %1)%!0)"),
                socket_filename);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-S", "--socket_file", OptionType::Value)) {
                socket_filename = opt.current_value;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    // Validate configuration
    {
        unsigned int flags = (int)rk_ConfigFlag::RequireAuth | (int)rk_ConfigFlag::RequireAgent;

        if (!rk_config.Complete())
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

    if (!server.Bind(socket_filename))
        return 1;
    server.Start(HandleClientData);

    // Make sure we can fetch plan (valid URL and credentials)
    if (!UpdatePlan(false))
        return 1;

    // From here on, don't quit abruptly
    WaitEvents(0);

    // Run two event loops: one for plan fetch and execution, one for clients.
    // Makes things easier, with a simple mutex for synchronization.
    std::thread thread([&]() {
        for (;;) {
            RunPlan();

            WaitResult ret = WaitEvents(rk_config.agent_period);

            if (ret == WaitResult::Exit || ret == WaitResult::Interrupt)
                break;
            K_ASSERT(ret != WaitResult::Message);

            UpdatePlan(true);
        }
    });
    K_DEFER { thread.join(); };

    // Handle clients (such as RekkordTray)
    int status = 0;
    for (;;) {
        uint64_t ready = 0;
        WaitResult ret = WaitEvents(server.GetWaitSources(), -1, &ready);

        if (ret == WaitResult::Exit) {
            LogInfo("Exit requested");
            break;
        } else if (ret == WaitResult::Interrupt) {
            LogInfo("Process interrupted");
            status = 1;
            break;
        }

        if (!server.Process(ready))
            return 1;

        if (ret == WaitResult::Message) {
            server.Send([](StreamWriter *writer) { return SendInfo(writer); });
        }
    }

    return status;
}

}
