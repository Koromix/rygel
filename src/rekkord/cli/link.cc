// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/base/tower.hh"
#include "rekkord.hh"
#include "link.hh"
#include "lib/native/request/curl.hh"
#include "lib/native/wrap/json.hh"

namespace K {

static bool SendReport(const char *web, const char *key, Span<const char> json)
{
    BlockAllocator temp_alloc;

    CURL *curl = curl_Init();
    if (!curl)
        return false;
    K_DEFER { curl_easy_cleanup(curl); };

    const char *url = Fmt(&temp_alloc, "%1/api/link/snapshot", TrimStrRight(web, '/')).ptr;

    curl_slist headers[] = {
        { (char *)"Content-Type: application/json", nullptr },
        { Fmt(&temp_alloc, "X-Api-Key: %1", key).ptr, nullptr }
    };
    headers[0].next = &headers[1];

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

bool ReportSnapshot(const char *url, const char *key, const char *repository, const char *channel, const rk_SaveInfo &info)
{
    HeapArray<char> body;

    char oid[128];
    Fmt(oid, "%1", info.oid);

    // Format JSON
    {
        StreamWriter st(&body, "<report>");
        json_Writer json(&st);

        json.StartObject();
        json.Key("repository"); json.String(repository);
        json.Key("channel"); json.String(channel);
        json.Key("timestamp"); json.Int64(info.time);
        json.Key("oid"); json.String(oid);
        json.Key("size"); json.Int64(info.size);
        json.Key("stored"); json.Int64(info.stored);
        json.Key("added"); json.Int64(info.added);
        json.EndObject();
    }

    return SendReport(url, key, body);
}

bool ReportError(const char *url, const char *key, const char *repository, const char *channel, int64_t time, const char *message)
{
    HeapArray<char> body;

    // Format JSON
    {
        StreamWriter st(&body, "<report>");
        json_Writer json(&st);

        json.StartObject();
        json.Key("repository"); json.String(repository);
        json.Key("channel"); json.String(channel);
        json.Key("timestamp"); json.Int64(time);
        json.Key("error"); json.String(message);
        json.EndObject();
    }

    return SendReport(url, key, body);
}

}
