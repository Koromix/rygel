// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "config.hh"
#include "src/core/libnet/libnet.hh"
#include "../../teensy/common/protocol.hh"
#include "vendor/libhs/libhs.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.h"
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <unistd.h>
#endif
#include <thread>

namespace RG {

struct Client {
    Client *prev;
    Client *next;

    StreamReader reader;
    StreamWriter writer;
};

static Config mael_config;

static HashMap<const char *, const AssetInfo *> assets_map;
static HeapArray<const char *> assets_for_cache;
static LinkedAllocator assets_alloc;
static char shared_etag[17];

static hs_monitor *monitor = nullptr;
static std::thread monitor_thread;
#ifdef _WIN32
static HANDLE monitor_event;
#else
static int monitor_pfd[2] = {-1, -1};
#endif

static std::mutex comm_mutex;
static hs_device *comm_dev = nullptr;
static hs_port *comm_port = nullptr;

enum class ReceptionStatus {
    None,
    Started,
    Complete
};

alignas(uint64_t) static LocalArray<uint8_t, 65536> recv_buf;
static ReceptionStatus recv_status = ReceptionStatus::None;
static Size recv_start = 0;
static Size recv_end = 0;

static std::mutex clients_mutex;
static Client clients_root = {&clients_root, &clients_root};

static const hs_match_spec DeviceSpecs[] = {
    HS_MATCH_VID_PID(0x16C0, 0x0483, nullptr)
};

static AssetInfo *PatchVariables(const AssetInfo &asset)
{
    AssetInfo *copy = AllocateMemory<AssetInfo>(&assets_alloc, RG_SIZE(AssetInfo)).ptr;

    *copy = asset;
    copy->data = PatchAsset(*copy, &assets_alloc, [](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(FelixVersion);
        } else if (TestStr(key, "COMPILER")) {
            writer->Write(FelixCompiler);
        } else if (TestStr(key, "PWA")) {
            writer->Write(mael_config.pwa ? "true" : "false");
        } else if (TestStr(key, "BUSTER")) {
            writer->Write(shared_etag);
        } else {
            Print(writer, "{%1}", key);
        }
    });

    return copy;
}

static void InitAssets()
{
    assets_map.Clear();
    assets_for_cache.Clear();
    assets_alloc.ReleaseAll();

    // Update ETag
    {
        uint64_t buf;
        FillRandomSafe(&buf, RG_SIZE(buf));
        Fmt(shared_etag, "%1", FmtHex(buf).Pad0(-16));
    }

    for (const AssetInfo &asset: GetPackedAssets()) {
        if (TestStr(asset.name, "src/mael/raspi/client/mael.html")) {
            AssetInfo *copy = PatchVariables(asset);
            assets_map.Set("/", copy);
            assets_for_cache.Append("/");
        } else if (TestStr(asset.name, "src/mael/raspi/client/assets/favicon.png")) {
            assets_map.Set("/favicon.png", &asset);
            assets_for_cache.Append("/favicon.png");
        } else if (TestStr(asset.name, "src/mael/raspi/client/manifest.json")) {
            assets_map.Set("/manifest.json", &asset);
            assets_for_cache.Append("/manifest.json");
        } else if (TestStr(asset.name, "src/mael/raspi/client/sw.pk.js")) {
            AssetInfo *copy = PatchVariables(asset);
            assets_map.Set("/sw.pk.js", copy);
        } else if (StartsWith(asset.name, "src/mael/raspi/client/") ||
                   StartsWith(asset.name, "vendor/")) {
            const char *name = SplitStrReverseAny(asset.name, RG_PATH_SEPARATORS).ptr;
            const char *url = Fmt(&assets_alloc, "/static/%1", name).ptr;

            assets_map.Set(url, &asset);
            assets_for_cache.Append(url);
        }
    }
}

static int DeviceCallback(hs_device *dev, void *)
{
    std::lock_guard<std::mutex> lock(comm_mutex);

    switch (dev->status) {
        case HS_DEVICE_STATUS_ONLINE: {
            if (!mael_config.serial_number || TestStr(dev->serial_number_string, mael_config.serial_number)) {
                if (comm_dev) {
                    LogError("Ignoring supplementary device '%1'", dev->location);
                    return 0;
                }

                LogInfo("Acquired control device '%1' (S/N = %2)", dev->location, dev->serial_number_string);
                comm_dev = hs_device_ref(dev);
            }
        } break;

        case HS_DEVICE_STATUS_DISCONNECTED: {
            if (dev == comm_dev) {
                LogInfo("Lost control device '%1'", dev->location);

                hs_device_unref(comm_dev);
                hs_port_close(comm_port);
                comm_dev = nullptr;
                comm_port = nullptr;
            }
        } break;
    }

    return 0;
}

static bool CheckIntegrity(Span<const uint8_t> pkt)
{
    const PacketHeader &hdr = *(const PacketHeader *)pkt.ptr;

    if (pkt.len < RG_SIZE(hdr)) {
        LogError("Truncated packet header");
        return false;
    }
    if (hdr.payload != pkt.len - RG_SIZE(PacketHeader)) {
        LogError("Invalid payload length");
        return false;
    }
    if (hdr.type >= RG_LEN(PacketSizes)) {
        LogError("Invalid packet type");
        return false;
    }
    if (hdr.payload != PacketSizes[hdr.type]) {
        LogError("Mis-sized packet payload");
        return false;
    }
    if (hdr.crc32 != mz_crc32(MZ_CRC32_INIT, pkt.ptr + 4, hdr.payload + 4)) {
        LogError("Packet failed CRC32 check");
        return false;
    }

    return true;
}

static void ReceivePacket()
{
    Span<uint8_t> pkt = {};

    // Find start marker
    if (recv_status == ReceptionStatus::None) {
        while (recv_start < recv_buf.len) {
            if (recv_buf[recv_start++] == 0xA) {
                recv_end = recv_start;
                recv_status = ReceptionStatus::Started;

                break;
            }
        }
    }

    // Complete packet
    if (recv_status == ReceptionStatus::Started) {
        while (recv_end < recv_buf.len) {
            if (recv_buf[recv_end++] == 0xA) {
                Size delta = recv_end - recv_start;
                memmove(recv_buf.data, recv_buf.data + recv_start, delta);

                pkt = MakeSpan(recv_buf.data, delta - 1);
                recv_status = ReceptionStatus::Complete;

                break;
            }
        }
    }

    // Process full packet
    if (recv_status == ReceptionStatus::Complete) {
        Size j = 0;
        for (Size i = 0; i < pkt.len; i++, j++) {
            if (pkt[i] == 0xD) {
                pkt[j] = pkt.ptr[++i] ^ 0x8;
            } else {
                pkt[j] = pkt[i];
            }
        }
        pkt.len = j;

        if (!pkt.len) {
            // Fix start/end inversion
            recv_start = 0;
            recv_end = 1;
        } else if (CheckIntegrity(pkt)) {
            std::lock_guard<std::mutex> lock(clients_mutex);

            for (Client *client = clients_root.next; client != &clients_root; client = client->next) {
                client->writer.Write(pkt);
            }
        }

        recv_buf.len -= recv_end;
        memmove(recv_buf.data, recv_buf.data + recv_end, recv_buf.len);

        recv_start = 0;
        recv_end = 0;
        recv_status = ReceptionStatus::None;
    }
}

static void RunMonitorThread()
{
    LocalArray<hs_poll_source, 3> sources = {
#ifdef _WIN32
        {monitor_event},
#else
        {monitor_pfd[0]},
#endif
        {hs_monitor_get_poll_handle(monitor)}
    };

    do {
        // Try to open device
        if (!comm_port) {
            std::lock_guard<std::mutex> lock(comm_mutex);

            hs_device *dev = comm_dev ? hs_device_ref(comm_dev) : nullptr;
            RG_DEFER { hs_device_unref(dev); };

            if (dev) {
                hs_port_open(dev, HS_PORT_MODE_RW, &comm_port);

                recv_buf.Clear();
                recv_start = 0;
                recv_end = 0;
                recv_status = ReceptionStatus::None;
            }
        }

        // Poll the controller if it is plugged
        sources.len = 2;
        if (comm_port) {
            hs_handle h = hs_port_get_poll_handle(comm_port);
            sources.Append({h});
        }

        // Wait for something to happen
        if (hs_poll(sources.data, (unsigned int)sources.len, -1) < 0) {
            SignalWaitFor();
            return;
        }

        // Refresh known devices
        if (sources[1].ready && hs_monitor_refresh(monitor, DeviceCallback, nullptr) < 0) {
            SignalWaitFor();
            return;
        }

        if (comm_port && sources[2].ready) {
            std::lock_guard<std::mutex> lock(comm_mutex);

            Span<uint8_t> buf = recv_buf.TakeAvailable();
            buf.len = hs_serial_read(comm_port, buf.ptr, buf.len, 0);

            if (buf.len >= 0) {
                recv_buf.len += buf.len;
                ReceivePacket();
            } else {
                hs_port_close(comm_port);
                comm_port = nullptr;
            }
        }
    } while (!sources[0].ready);
}

static void StopMonitor();
static bool InitMonitor()
{
    RG_DEFER_N(err_guard) { StopMonitor(); };

#ifdef _WIN32
    monitor_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!monitor_event) {
        LogError("CreateEvent() failed: %1", GetWin32ErrorString());
        return false;
    }
#else
    if (!CreatePipe(monitor_pfd))
        return false;
#endif

    if (mael_config.serial_number) {
        LogInfo("Expecting relay device serial number '%!..+%1%!0'", mael_config.serial_number);
    } else {
        LogInfo("Expecting relay device with any serial number");
    }

    if (hs_monitor_new(DeviceSpecs, RG_LEN(DeviceSpecs), &monitor) < 0)
        return false;
    if (hs_monitor_start(monitor) < 0)
        return false;

    if (hs_monitor_list(monitor, DeviceCallback, nullptr) < 0)
        return false;
    monitor_thread = std::thread(RunMonitorThread);

    err_guard.Disable();
    return true;
}

static void StopMonitor()
{
    if (monitor) {
#ifdef _WIN32
        SetEvent(monitor_event);
#else
        char dummy = 0;
        RG_IGNORE write(monitor_pfd[1], &dummy, 1);
#endif

        if (monitor_thread.joinable()) {
            monitor_thread.join();
        }
        monitor_thread = {};

        hs_monitor_stop(monitor);
    }

    hs_monitor_free(monitor);
    monitor = nullptr;

#ifdef _WIN32
    if (monitor_event) {
        CloseHandle(monitor_event);
        monitor_event = nullptr;
    }
#else
    close(monitor_pfd[0]);
    close(monitor_pfd[1]);
    monitor_pfd[0] = -1;
    monitor_pfd[1] = -1;
#endif

    hs_port_close(comm_port);
    hs_device_unref(comm_dev);
    comm_dev = nullptr;
    comm_port = nullptr;
}

static void RelayPacketToDevice(Span<const uint8_t> pkt)
{
    RG_ASSERT(pkt.len <= 1024);

    std::lock_guard<std::mutex> lock(comm_mutex);

    if (!comm_port) {
        LogError("Dropping packet (device not open)");
        return;
    }

    LocalArray<uint8_t, 4096> buf;
    buf.Append(0xA);
    for (uint8_t c: pkt) {
        if (c == 0xA || c == 0xD) {
            buf.Append(0xD);
            c ^= 0x8;
        }
        buf.Append(c);
    }
    buf.Append(0xA);

#if 0
    // Dump packet
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        Print(stderr, "PKT:");
        for (uint8_t c: buf) {
            Print(" 0x%1", FmtHex(c).Pad0(-2));
        }
        PrintLn(stderr);
    }
#endif

    // Do something if it fails?
    hs_serial_write(comm_port, buf.data, (size_t)buf.len, -1);
}

static void HandleWebSocket(const http_RequestInfo &request, http_IO *io)
{
    io->RunAsync([=]() {
        Client client;

        // Upgrade connection
        if (!io->UpgradeToWS(0))
            return;
        io->OpenForReadWS(&client.reader);
        io->OpenForWriteWS(&client.writer);

        // Register client
        {
            std::lock_guard<std::mutex> lock(clients_mutex);

            client.prev = clients_root.prev;
            client.prev->next = &client;
            clients_root.prev = &client;
            client.next = &clients_root;
        }
        RG_DEFER {
            std::lock_guard<std::mutex> lock(clients_mutex);

            client.next->prev = client.prev;
            client.prev->next = client.next;
        };

        // Transmit commands to control device
        while (!client.reader.IsEOF()) {
            alignas(uint64_t) LocalArray<uint8_t, 1024> buf;
            buf.len = client.reader.Read(buf.data);
            if (buf.len <= 0)
                break;

            if (CheckIntegrity(buf)) {
                RelayPacketToDevice(buf);
            }
        }
    });
}

static void AttachStatic(const AssetInfo &asset, int max_age, const char *etag,
                         const http_RequestInfo &request, http_IO *io)
{
    const char *client_etag = request.GetHeaderValue("If-None-Match");

    if (client_etag && TestStr(client_etag, etag)) {
        MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        io->AttachResponse(304, response);
    } else {
        const char *mimetype = http_GetMimeType(GetPathExtension(asset.name));

        io->AttachBinary(200, asset.data, mimetype, asset.compression_type);
        io->AddCachingHeaders(max_age, etag);

        if (asset.source_map) {
            io->AddHeader("SourceMap", asset.source_map);
        }
    }
}

static void HandleAppStatic(const http_RequestInfo &, http_IO *io)
{
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    for (const char *url: assets_for_cache) {
        json.String(url);
    }
    json.EndArray();

    json.Finish();

    io->AddCachingHeaders(0, nullptr);
}

static void HandleRequest(const http_RequestInfo &request, http_IO *io)
{
#ifdef FELIX_HOT_ASSETS
    // This is not actually thread safe, because it may release memory from an asset
    // that is being used by another thread. This code only runs in development builds
    // and it pretty much never goes wrong so it is kind of OK.
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        if (ReloadAssets()) {
            LogInfo("Reload assets");
            InitAssets();
        }
    }
#endif

    if (mael_config.require_host) {
        const char *host = request.GetHeaderValue("Host");

        if (!host) {
            LogError("Request is missing required Host header");
            io->AttachError(400);
            return;
        }
        if (!TestStr(host, mael_config.require_host)) {
            LogError("Unexpected Host header '%1'", host);
            io->AttachError(403);
            return;
        }
    }

    // Send these headers whenever possible
    io->AddHeader("Referrer-Policy", "no-referrer");
    io->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    io->AddHeader("X-Robots-Tag", "noindex");
    io->AddHeader("Permissions-Policy", "interest-cohort=()");

    // Try static assets first
    {
        const AssetInfo *asset = assets_map.FindValue(request.url, nullptr);

        if (asset) {
            AttachStatic(*asset, 0, shared_etag, request, io);
            return;
        };
    }

    // Try API endpoints
    if (TestStr(request.url, "/api/static")) {
        HandleAppStatic(request, io);
    } else if (TestStr(request.url, "/api/ws")) {
        HandleWebSocket(request, io);
    } else {
        io->AttachError(404);
    }
}

int Main(int argc, char **argv)
{
    // Options
    const char *config_filename = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-s, --serial_number <S/N>%!0    Set expected serial number
        %!..+--pwa%!0                    Enable PWA mode

        %!..+--port <port>%!0            Change web server port
                                 %!D..(default: %2)%!0)",
                FelixTarget, mael_config.http.port);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Find config filename
    {
        OptionParser opt(argc, argv, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Load config file
    if (config_filename && !LoadConfig(config_filename, &mael_config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-s", "--serial_number", OptionType::Value)) {
                mael_config.serial_number = opt.current_value;
            } else if (opt.Test("--pwa")) {
                mael_config.pwa = true;
            } else if (opt.Test("--port", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &mael_config.http.port))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    // Init assets
    LogInfo("Init assets");
    InitAssets();

    // Init device access
    LogInfo("Init device monitor");
    hs_log_set_handler([](hs_log_level level, int, const char *msg, void *) {
        switch (level) {
            case HS_LOG_ERROR:
            case HS_LOG_WARNING: { LogError("%1", msg); } break;
            case HS_LOG_DEBUG: { LogDebug("%1", msg); } break;
        }
    }, nullptr);
    if (!InitMonitor())
        return 1;
    RG_DEFER { StopMonitor(); };

    // Run!
    LogInfo("Init HTTP server");
    http_Daemon daemon;
    if (!daemon.Start(mael_config.http, HandleRequest))
        return 1;

#ifdef __linux__
    if (!NotifySystemd())
        return 1;
#endif

    // Run until exit
    if (WaitForInterrupt() == WaitForResult::Interrupt) {
        LogInfo("Exit requested");
    }
    LogDebug("Stop HTTP server");
    daemon.Stop();

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
