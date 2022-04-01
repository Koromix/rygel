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

#ifdef __linux__

#include "src/core/libcc/libcc.hh"
#include "config.hh"
#include "src/core/libsandbox/libsandbox.hh"
#include "vendor/basu/src/systemd/sd-bus.h"
#include "vendor/stb/stb_image.h"
#include "vendor/stb/stb_image_resize.h"

#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace RG {

#define CALL_SDBUS(Call, Message, Ret) \
    do { \
        int ret = Call; \
         \
        if (ret < 0) { \
            LogError("%1: %2", (Message), strerror(-ret)); \
            return (Ret); \
        } \
    } while (false)

extern "C" const AssetInfo MeesticPng;

static const Vec2<int> IconSizes[] = {
    { 24, 24 },
    { 32, 32 },
    { 64, 64 },
    { 128, 128 },
    { 256, 256 }
};

static int meestic_fd = -1;

static Config config;

static char bus_name[512];
static sd_bus *bus_sys;
static sd_bus *bus_user;

static bool ApplyProfile(Size idx)
{
    LogInfo("Applying profile %1", idx);

    uint8_t payload = (uint8_t)idx;

    if (send(meestic_fd, &payload, 1, 0) < 0) {
        LogError("Failed to send message to server: %1", strerror(errno));
        return false;
    }

    return true;
}

static bool ToggleProfile(int delta)
{
    if (!delta)
        return true;

    uint8_t payload = delta > 0 ? 0x81 : 0x80;

    if (send(meestic_fd, &payload, 1, 0) < 0) {
        LogError("Failed to send message to server: %1", strerror(errno));
        return false;
    }

    return true;
}

static const Span<const uint8_t> *InitIcons()
{
    static bool init = false;
    static Span<const uint8_t> icons[RG_LEN(IconSizes)];
    static LinkedAllocator icons_alloc;

    // We could do this at compile-time if we had better (or at least easier to use) compile-time
    // possibilities... Well. It's quick enough, so no big deal.
    if (!init) {
        RG_ASSERT(MeesticPng.compression_type == CompressionType::None);

        // Load embedded PNG file
        int width, height, channels;
        uint8_t *png = stbi_load_from_memory(MeesticPng.data.ptr, MeesticPng.data.len, &width, &height, &channels, 4);
        RG_CRITICAL(png, "Failed to load embedded PNG icon");
        RG_DEFER { free(png); };

        for (Size i = 0; i < RG_LEN(IconSizes); i++) {
            Vec2<int> size = IconSizes[i];

            // Allocate memory
            Size len = 4 * size.x * size.y;
            uint8_t *icon = (uint8_t *)AllocateRaw(&icons_alloc, len);
            icons[i] = MakeSpan(icon, len);

            // Downsize the icon for the tray
            int resized = stbir_resize_uint8(png, width, height, 0, icon, size.x, size.y, 0, 4);
            RG_CRITICAL(resized, "Failed to resize icon");

            // Convert from RGBA32 (Big Endian) to ARGB32 (Big Endian)
            for (Size i = 0; i < len; i += 4) {
                uint32_t pixel = LittleEndian(*(uint32_t *)(icon + i));

                icon[i + 0] = (uint8_t)((pixel >> 24) & 0xFF);
                icon[i + 1] = (uint8_t)((pixel >> 0) & 0xFF);
                icon[i + 2] = (uint8_t)((pixel >> 8) & 0xFF);
                icon[i + 3] = (uint8_t)((pixel >> 16) & 0xFF);
            }
        }

        init = true;
    }

    return icons;
}

static int GetComplexProperty(sd_bus *, const char *, const char *, const char *property,
                              sd_bus_message *reply, void *, sd_bus_error *)
{
    static const char *error = "Failed to prepare sd-bus reply";

    if (TestStr(property, "ToolTip")) {
        const Span<const uint8_t> *icons = InitIcons();

        CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "sa(iiay)ss"), error, -1);
        CALL_SDBUS(sd_bus_message_append(reply, "s", "MeesticGui"), error, -1);
        CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(iiay)"), error, -1);
        for (Size i = 0; i < RG_LEN(IconSizes); i++) {
            Vec2<int> size = IconSizes[i];
            Span<const uint8_t> icon = icons[i];

            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "iiay"), error, -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ii", size.x, size.y), error, -1);
            CALL_SDBUS(sd_bus_message_append_array(reply, 'y', icon.ptr, (size_t)icon.len), error, -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), error, -1);
        }
        CALL_SDBUS(sd_bus_message_close_container(reply), error, -1);
        CALL_SDBUS(sd_bus_message_append(reply, "ss", FelixTarget, FelixTarget), error, -1);
        CALL_SDBUS(sd_bus_message_close_container(reply), error, -1);

        return 1;
    } else if (TestStr(property, "IconPixmap")) {
        const Span<const uint8_t> *icons = InitIcons();

        CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(iiay)"), error, -1);
        for (Size i = 0; i < RG_LEN(IconSizes); i++) {
            Vec2<int> size = IconSizes[i];
            Span<const uint8_t> icon = icons[i];

            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "iiay"), error, -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ii", size.x, size.y), error, -1);
            CALL_SDBUS(sd_bus_message_append_array(reply, 'y', icon.ptr, (size_t)icon.len), error, -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), error, -1);
        }
        CALL_SDBUS(sd_bus_message_close_container(reply), error, -1);

        return 1;
    }

    RG_UNREACHABLE();
}

static int HandleMatch(sd_bus_message *m, void *, sd_bus_error *)
{
    const char *name;
    CALL_SDBUS(sd_bus_message_read(m, "s", &name), "Failed to parse arguments", -1);

    if (TestStr(name, "org.kde.StatusNotifierWatcher")) {
        CALL_SDBUS(sd_bus_call_method(bus_user, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                                      "RegisterStatusNotifierItem", nullptr, nullptr, "s", bus_name),
                   "Failed to register tray icon item with the watcher", -1);
        return 1;
    }

    return 1;
}

static bool RegisterTrayIcon()
{
    RG_ASSERT(!bus_name[0]);
    Fmt(bus_name, "org.kde.StatusNotifierItem-%1-1", getpid());

    CALL_SDBUS(sd_bus_request_name(bus_user, bus_name, 0), "Failed to acquire tray icon name", false);

    static struct {
        const char *category = "ApplicationStatus";
        const char *id = FelixTarget;
        const char *title = FelixTarget;
        const char *status = "Passive";
        uint32_t window_id = 0;
        const char *icon_theme = "";
        const char *icon_name = "meesticgui";
        bool item_is_menu = false;
        const char *menu = "/ContextMenu";
     } properties;

    static const sd_bus_vtable vtable[] = {
        SD_BUS_VTABLE_START(0),

        SD_BUS_PROPERTY("Category", "s", nullptr, RG_OFFSET_OF(decltype(properties), category), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("Id", "s", nullptr, RG_OFFSET_OF(decltype(properties), id), 0),
        SD_BUS_PROPERTY("Title", "s", nullptr, RG_OFFSET_OF(decltype(properties), title), 0),
        SD_BUS_PROPERTY("Status", "s", nullptr, RG_OFFSET_OF(decltype(properties), status), 0),
        SD_BUS_PROPERTY("WindowId", "u", nullptr, RG_OFFSET_OF(decltype(properties), window_id), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("IconThemePath", "s", nullptr, RG_OFFSET_OF(decltype(properties), icon_theme), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("IconName", "s", nullptr, RG_OFFSET_OF(decltype(properties), icon_name), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("IconPixmap", "a(iiay)", GetComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", GetComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("ItemIsMenu", "b", nullptr, RG_OFFSET_OF(decltype(properties), item_is_menu), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("Menu", "o", nullptr, RG_OFFSET_OF(decltype(properties), menu), SD_BUS_VTABLE_PROPERTY_CONST),

        SD_BUS_METHOD("Activate", "ii", "", [](sd_bus_message *, void *, sd_bus_error *) {
            ToggleProfile(1);
            return 1;
        }, 0),
        SD_BUS_METHOD("Scroll", "is", "", [](sd_bus_message *m, void *, sd_bus_error *) {
            static int64_t last_time = -50;
            int64_t now = GetMonotonicTime();

            if (now - last_time >= 50) {
                last_time = now;

                int delta;
                const char *orientation;
                CALL_SDBUS(sd_bus_message_read(m, "is", &delta, &orientation), "Failed to parse arguments", -1);

                if (TestStrI(orientation, "vertical")) {
                    delta = std::clamp(delta, -1, 1);
                    ToggleProfile(delta);
                }
            }

            return 1;
        }, 0),

        SD_BUS_SIGNAL("NewTitle", "", 0),
        SD_BUS_SIGNAL("NewIcon", "", 0),
        SD_BUS_SIGNAL("NewAttentionIcon", "", 0),
        SD_BUS_SIGNAL("NewOverlayIcon", "", 0),
        SD_BUS_SIGNAL("NewToolTip", "", 0),
        SD_BUS_SIGNAL("NewStatus", "s", 0),

        SD_BUS_VTABLE_END
    };

    CALL_SDBUS(sd_bus_add_object_vtable(bus_user, nullptr, "/StatusNotifierItem", "org.kde.StatusNotifierItem", vtable, &properties),
               "Failed to create tray icon object", false);
    CALL_SDBUS(sd_bus_match_signal(bus_user, nullptr, "org.freedesktop.DBus", nullptr, "org.freedesktop.DBus",
                                   "NameOwnerChanged", HandleMatch, nullptr),
               "Failed to add D-Bus match rule", false);

    // Ignore failure... maybe the watcher is not ready yet?
    sd_bus_call_method(bus_user, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                      "RegisterStatusNotifierItem", nullptr, nullptr, "s", bus_name);

    return true;
}

static int GetBusTimeout(sd_bus *bus)
{
    uint64_t timeout64;
    CALL_SDBUS(sd_bus_get_timeout(bus, &timeout64), "Failed to get D-Bus connection timeout", -1);

    int timeout = (int)std::min(timeout64 / 1000, (uint64_t)INT_MAX);
    return timeout;
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *socket_filename = "/run/meestic.sock";

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 [options]%!0

Options:
    %!..+-S, --socket_file <socket>%!0   Change control socket
                                 %!D..(default: %3)%!0)",
                FelixTarget, socket_filename);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-S", "--socket_file", OptionType::Value)) {
                socket_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Open Meestic socket
    meestic_fd = ConnectToUnixSocket(socket_filename);
    if (meestic_fd < 0)
        return 1;
    RG_DEFER { close(meestic_fd); };

    // Read config from server
    {
        auto read = [&](Span<uint8_t> out_buf) { return recv(meestic_fd, out_buf.ptr, out_buf.len, 0); };
        StreamReader reader(read, "<meestic>");

        if (!LoadConfig(&reader, &config))
            return 1;
    }

    // Open D-Bus connections
    RG_DEFER {
        sd_bus_flush_close_unref(bus_sys);
        sd_bus_flush_close_unref(bus_user);
    };
    CALL_SDBUS(sd_bus_open_system_with_description(&bus_sys, FelixTarget), "Failed to connect to system D-Bus bus", 1);
    CALL_SDBUS(sd_bus_open_user_with_description(&bus_user, FelixTarget), "Failed to connect to session D-Bus bus", 1);

    // Register the tray icon
    if (!RegisterTrayIcon())
        return 1;

    // From here on, don't quit abruptly
    WaitForInterrupt(0);

    // React to main service and D-Bus events
    for (;;) {
        struct pollfd pfds[3] = {
            { meestic_fd, 0, 0 },
            { sd_bus_get_fd(bus_sys), (short)sd_bus_get_events(bus_sys), 0 },
            { sd_bus_get_fd(bus_user), (short)sd_bus_get_events(bus_user), 0 }
        };

        int timeout = std::min(GetBusTimeout(bus_sys), GetBusTimeout(bus_user));
        if (timeout < 0)
            return 1;
        if (timeout == INT_MAX)
            timeout = -1;

        if (poll(pfds, RG_LEN(pfds), timeout) < 0) {
            if (errno == EINTR) {
                if (WaitForResult(0) == WaitForResult::Interrupt) {
                    break;
                } else {
                    continue;
                }
            }

            LogError("Failed to poll I/O descriptors: %1", strerror(errno));
            return 1;
        }

        if (pfds[0].revents & (POLLERR | POLLHUP)) {
            LogError("Lost connection to server");
            return 1;
        }

        CALL_SDBUS(sd_bus_process(bus_sys, nullptr), "Failed to process system D-Bus messages", 1);
        CALL_SDBUS(sd_bus_process(bus_user, nullptr), "Failed to process session D-Bus messages", 1);
    }

    return 0;
}

#undef CALL_SDBUS

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }

#endif
