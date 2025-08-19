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

namespace RG {

struct PlanItem {
    const char *channel;
    int clock;
    int days;
    HeapArray<const char *> paths;

    int64_t timestamp;
    bool success;
};

static void LinkHeaders(Span<curl_slist> headers)
{
    if (!headers.len)
        return;

    for (Size i = 0; i < headers.len - 1; i++) {
        headers[i].next = &headers[i + 1];
    }
    headers[headers.len - 1].next = nullptr;
}

static bool FetchPlan(Allocator *alloc, HeapArray<PlanItem> *out_items)
{
    RG_DEFER_NC(err_guard, len = out_items->len) { out_items->RemoveFrom(len); };

    Span<const char> api = rekkord_config.agent_url;
    const char *key = rekkord_config.api_key;

    HeapArray<char> json;
    {
        CURL *curl = curl_Init();
        if (!curl)
            return false;
        RG_DEFER { curl_easy_cleanup(curl); };

        const char *url = Fmt(alloc, "%1/api/plan/fetch", TrimStrRight(api, '/')).ptr;

        curl_slist headers[] = {
            { Fmt(alloc, "X-Api-Key: %1", key).ptr, nullptr }
        };
        LinkHeaders(headers);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, &headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *udata) {
            HeapArray<char> *json = (HeapArray<char> *)udata;

            Span<const char> buf = MakeSpan(ptr, (Size)nmemb);
            json->Append(buf);

            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json);

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
        StreamReader st(json.As<uint8_t>());
        json_Parser parser(&st, alloc);

        parser.ParseArray();
        while (parser.InArray()) {
            PlanItem item = {};

            parser.ParseObject();
            while (parser.InObject()) {
                Span<const char> key = {};
                parser.ParseKey(&key);

                if (key == "id") {
                    parser.Skip();
                } else if (key == "channel") {
                    parser.ParseString(&item.channel);
                } else if (key == "clock") {
                    parser.ParseInt(&item.clock);
                } else if (key == "days") {
                    parser.ParseInt(&item.days);
                } else if (key == "timestamp") {
                    parser.SkipNull() || parser.ParseInt(&item.timestamp);
                } else if (key == "success") {
                    parser.ParseBool(&item.success);
                } else if (key == "paths") {
                    parser.ParseArray();
                    while (parser.InArray()) {
                        const char *path = nullptr;
                        parser.ParseString(&path);

                        item.paths.Append(path);
                    }
                } else if (parser.IsValid()) {
                    LogError("Unexpected key '%1'", key);
                    return false;
                }
            }

            out_items->Append(item);
        }
        if (!parser.IsValid())
            return false;
    }

    err_guard.Disable();
    return true;
}

static bool ShouldRun(const PlanItem &item)
{
    RG_ASSERT(item.days & 0b1111111);

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

    Span<const char> api = rekkord_config.agent_url;
    const char *key = rekkord_config.api_key;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    RG_DEFER { curl_easy_cleanup(curl); };

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

static bool ReportSuccess(const char *channel, const rk_SaveInfo &info)
{
    HeapArray<char> body;

    char oid[128];
    Fmt(oid, "%1", info.oid);

    // Format JSON
    {
        StreamWriter st(&body);
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
        StreamWriter st(&body);
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
    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return false;

    return rk_Save(repo.get(), channel, paths, {}, out_info);
}

static bool CheckPlan()
{
    BlockAllocator temp_alloc;

    HeapArray<PlanItem> items;
    bool nothing = true;

    LogInfo("Fetching backup plan...");
    if (!FetchPlan(&temp_alloc, &items))
        return false;

    for (const PlanItem &item: items) {
        bool run = ShouldRun(item);

        if (run) {
            LogInfo("Running snapshot for '%1'", item.channel);

            const char *last_err = "Unknown error";
            PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
                if (level == LogLevel::Error) {
                    last_err = DuplicateString(msg, &temp_alloc).ptr;
                }

                func(level, ctx, msg);
            });
            RG_DEFER { PopLogFilter(); };

            rk_SaveInfo info = {};
            bool success = RunSnapshot(item.channel, item.paths, &info);

            if (success) {
                ReportSuccess(item.channel, info);
            } else {
                int64_t now = GetUnixTime();
                ReportError(item.channel, now, last_err);
            }
        }

        nothing &= !run;
    }
    if (nothing) {
        LogInfo("Nothing to do");
    }

    return true;
}

int RunAgent(Span<const char *> arguments)
{
    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 agent [-C filename] [option...]%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    // Validate configuration
    {
        unsigned int flags = (int)rk_ConfigFlag::RequireAuth | (int)rk_ConfigFlag::RequireAgent;

        if (!rekkord_config.Complete(flags))
            return 1;
        if (!rekkord_config.Validate(flags))
            return 1;
    }

    // Check repository connection
    {
        std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
        std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
        if (!repo)
            return 1;

        LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
        if (!repo->HasMode(rk_AccessMode::Read)) {
            LogError("Cannot restore data with %1 keys", repo->GetRole());
            return 1;
        }
        LogInfo();
    }

    // Check plan once at start
    if (!CheckPlan())
        return 1;

    // From here on, don't quit abruptly
    WaitForInterrupt(0);

    int status = 0;

    // Run periodic tasks until exit
    for (;;) {
        WaitForResult ret = WaitForInterrupt(rekkord_config.agent_period);

        if (ret == WaitForResult::Exit) {
            LogInfo("Exit requested");
            break;
        } else if (ret == WaitForResult::Interrupt) {
            LogInfo("Process interrupted");
            status = 1;
            break;
        }

        CheckPlan();
    }

    return status;
}

}
