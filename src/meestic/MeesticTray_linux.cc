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

#if defined(__linux__)

#include "src/core/base/base.hh"
#include "config.hh"
#include "light.hh"
#include "src/core/wrap/json.hh"
#include "vendor/basu/src/systemd/sd-bus.h"
#include "vendor/stb/stb_image.h"
#include "vendor/stb/stb_image_resize2.h"

#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

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

static const char *const ParseError = "Failed to parse D-bus message";
static const char *const ReplyError = "Failed to reply on D-bus";

static const Vec2<int> IconSizes[] = {
    { 22, 22 },
    { 48, 48 },
    { 64, 64 }
};

static int meestic_fd = -1;
static bool run = true;

static HeapArray<const char *> profiles;
static BlockAllocator profiles_alloc;
static unsigned int profiles_revision = 0;
static Size profile_idx = -1;

static char bus_name[512];
static sd_bus *bus_user;

static bool ApplyProfile(Size idx)
{
    LogInfo("Applying profile %1", idx);

    LocalArray<char, 128> buf;
    buf.len = Fmt(buf.data, "{\"apply\": %1}\n", idx).len;

    if (send(meestic_fd, buf.data, (size_t)buf.len, 0) < 0) {
        LogError("Failed to send message to server: %1", strerror(errno));
        return false;
    }

    return true;
}

static bool ToggleProfile(int delta)
{
    if (!delta)
        return true;

    LocalArray<char, 128> buf;
    buf.len = Fmt(buf.data, "{\"toggle\": \"%1\"}\n", delta > 0 ? "next" : "previous").len;

    if (send(meestic_fd, buf.data, (size_t)buf.len, 0) < 0) {
        LogError("Failed to send message to server: %1", strerror(errno));
        return false;
    }

    return true;
}

struct IconInfo {
    const char *filename;
    Span<const Span<const uint8_t>> pixmaps;
};

// Expects RGBA32
static void GeneratePNG(const uint8_t *data, int32_t width, int32_t height, HeapArray<uint8_t> *out_png)
{
    static const uint8_t header[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    static const uint8_t footer[] = { 0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};

    out_png->Append(header);

#pragma pack(push, 1)
    struct ChunkHeader {
        uint32_t len;
        uint8_t type[4];
    };
    struct IHDR {
        uint32_t width;
        uint32_t height;
        uint8_t bit_depth;
        uint8_t color_type;
        uint8_t compression;
        uint8_t filter;
        uint8_t interlace;
    };
#pragma pack(pop)

    // Write IHDR chunk
    {
        Size chunk_pos = out_png->len;

        ChunkHeader chunk = {};
        IHDR ihdr = {};

        chunk.len = BigEndian((uint32_t)RG_SIZE(ihdr));
        MemCpy(chunk.type, "IHDR", 4);
        ihdr.width = BigEndian(width);
        ihdr.height = BigEndian(width);
        ihdr.bit_depth = 8;
        ihdr.color_type = 6;
        ihdr.compression = 0;
        ihdr.filter = 0;
        ihdr.interlace = 0;

        out_png->Append(MakeSpan((const uint8_t *)&chunk, RG_SIZE(chunk)));
        out_png->Append(MakeSpan((const uint8_t *)&ihdr, RG_SIZE(ihdr)));

        // Chunk CRC-32
        Span<const uint8_t> span = MakeSpan(out_png->ptr + chunk_pos + 4, RG_SIZE(ihdr) + 4);
        uint32_t crc32 = BigEndian((uint32_t)CRC32(0, span));
        out_png->Append(MakeSpan((const uint8_t *)&crc32, 4));
    }

    // Write image data (IDAT)
    {
        Size chunk_pos = out_png->len;

        ChunkHeader chunk = {};
        chunk.len = 0; // Unknown for now
        MemCpy(chunk.type, "IDAT", 4);
        out_png->Append(MakeSpan((const uint8_t *)&chunk, RG_SIZE(chunk)));

        StreamWriter writer(out_png, "<png>", CompressionType::Zlib);
        for (int y = 0; y < height; y++) {
            writer.Write((uint8_t)0); // Scanline filter

            Span<const uint8_t> scanline = MakeSpan(data + 4 * y * width, 4 * width);
            writer.Write(scanline);
        }
        bool success = writer.Close();
        RG_ASSERT(success);

        // Fix length
        {
            uint32_t len = BigEndian((uint32_t)(out_png->len - chunk_pos - 8));
            uint32_t *ptr = (uint32_t *)(out_png->ptr + chunk_pos);
            MemCpy(ptr, &len, RG_SIZE(len));
        }

        // Chunk CRC-32
        Span<const uint8_t> span = MakeSpan(out_png->ptr + chunk_pos + 4, out_png->len - chunk_pos - 4);
        uint32_t crc32 = BigEndian((uint32_t)CRC32(0, span));
        out_png->Append(MakeSpan((const uint8_t *)&crc32, 4));
    }

    // End image (IEND)
    out_png->Append(footer);
}

static IconInfo InitIcons()
{
    static bool init = false;
    static Span<const uint8_t> icons[RG_LEN(IconSizes)];
    static IconInfo info;
    static BlockAllocator icons_alloc;

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
            uint8_t *resized = stbir_resize_uint8_linear(png, width, height, 0, icon, size.x, size.y, 0, STBIR_RGBA);
            RG_CRITICAL(resized, "Failed to resize icon");

            // Write out first icon size to disk
            if (!i) {
                const char *filename = GetUserCachePath("meestic/tray.png", &icons_alloc);

                if (filename && EnsureDirectoryExists(filename)) {
                    HeapArray<uint8_t> png;
                    GeneratePNG(icon, size.x, size.y, &png);

                    if (WriteFile(png, filename)) {
                        info.filename = filename;
                    } else {
                        UnlinkFile(filename);
                    }
                }
            }

            // Convert from RGBA32 (Big Endian) to ARGB32 (Big Endian)
            for (Size i = 0; i < len; i += 4) {
                uint32_t pixel = LittleEndian(*(uint32_t *)(icon + i));

                icon[i + 0] = (uint8_t)((pixel >> 24) & 0xFF);
                icon[i + 1] = (uint8_t)((pixel >> 0) & 0xFF);
                icon[i + 2] = (uint8_t)((pixel >> 8) & 0xFF);
                icon[i + 3] = (uint8_t)((pixel >> 16) & 0xFF);
            }
        }

        info.pixmaps = icons;

        init = true;
    }

    return info;
}

static int GetIconComplexProperty(sd_bus *, const char *, const char *, const char *property,
                                  sd_bus_message *reply, void *, sd_bus_error *)
{
    if (TestStr(property, "ToolTip")) {
        IconInfo icons = InitIcons();

        CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "sa(iiay)ss"), ReplyError, -1);
        CALL_SDBUS(sd_bus_message_append(reply, "s", FelixTarget), ReplyError, -1);
        CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(iiay)"), ReplyError, -1);
        for (Size i = 0; i < RG_LEN(IconSizes); i++) {
            Vec2<int> size = IconSizes[i];
            Span<const uint8_t> icon = icons.pixmaps[i];

            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "iiay"), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ii", size.x, size.y), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_append_array(reply, 'y', icon.ptr, (size_t)icon.len), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), ReplyError, -1);
        }
        CALL_SDBUS(sd_bus_message_close_container(reply), ReplyError, -1);
        CALL_SDBUS(sd_bus_message_append(reply, "ss", FelixTarget, FelixTarget), ReplyError, -1);
        CALL_SDBUS(sd_bus_message_close_container(reply), ReplyError, -1);

        return 1;
    } else if (TestStr(property, "IconName")) {
        IconInfo icons = InitIcons();

        CALL_SDBUS(sd_bus_message_append(reply, "s", icons.filename), ReplyError, -1);

        return 1;
    } else if (TestStr(property, "IconPixmap")) {
        IconInfo icons = InitIcons();

        CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(iiay)"), ReplyError, -1);
        for (Size i = 0; i < RG_LEN(IconSizes); i++) {
            Vec2<int> size = IconSizes[i];
            Span<const uint8_t> icon = icons.pixmaps[i];

            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "iiay"), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ii", size.x, size.y), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_append_array(reply, 'y', icon.ptr, (size_t)icon.len), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), ReplyError, -1);
        }
        CALL_SDBUS(sd_bus_message_close_container(reply), ReplyError, -1);

        return 1;
    } else if (TestStr(property, "AttentionIconPixmap")) {
        CALL_SDBUS(sd_bus_message_append(reply, "a(iiay)", 0), ReplyError, -1);
        return 1;
    } else if (TestStr(property, "OverlayIconPixmap")) {
        CALL_SDBUS(sd_bus_message_append(reply, "a(iiay)", 0), ReplyError, -1);
        return 1;
    }

    RG_UNREACHABLE();
}

static int HandleMatch(sd_bus_message *m, void *, sd_bus_error *)
{
    const char *name;
    CALL_SDBUS(sd_bus_message_read(m, "s", &name), ParseError, -1);

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
    Fmt(bus_name, "org.kde.StatusNotifierItem-%1-1", getpid());

    CALL_SDBUS(sd_bus_request_name(bus_user, bus_name, 0), "Failed to acquire tray icon name", false);

    static struct {
        const char *category = "ApplicationStatus";
        const char *id = FelixTarget;
        const char *title = FelixTarget;
        const char *status = "Active";
        uint32_t window_id = 0;
        const char *icon_theme = "";
        bool item_is_menu = false;
        const char *attention_icon_name = "";
        const char *attention_movie_name = "";
        const char *overlay_icon_name = "";
        const char *menu = "/MenuBar";
     } properties;

    static const sd_bus_vtable vtable[] = {
        SD_BUS_VTABLE_START(0),

        SD_BUS_PROPERTY("Category", "s", nullptr, offsetof(decltype(properties), category), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("Id", "s", nullptr, offsetof(decltype(properties), id), 0),
        SD_BUS_PROPERTY("Title", "s", nullptr, offsetof(decltype(properties), title), 0),
        SD_BUS_PROPERTY("Status", "s", nullptr, offsetof(decltype(properties), status), 0),
        SD_BUS_PROPERTY("WindowId", "u", nullptr, offsetof(decltype(properties), window_id), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("IconThemePath", "s", nullptr, offsetof(decltype(properties), icon_theme), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("IconName", "s", GetIconComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("IconPixmap", "a(iiay)", GetIconComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("AttentionIconName", "s", nullptr, offsetof(decltype(properties), attention_icon_name), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("AttentionMovieName", "s", nullptr, offsetof(decltype(properties), attention_movie_name), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("AttentionIconPixmap", "a(iiay)", GetIconComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("OverlayIconName", "s", nullptr, offsetof(decltype(properties), attention_icon_name), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("OverlayIconPixmap", "a(iiay)", GetIconComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", GetIconComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("ItemIsMenu", "b", nullptr, offsetof(decltype(properties), item_is_menu), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("Menu", "o", nullptr, offsetof(decltype(properties), menu), SD_BUS_VTABLE_PROPERTY_CONST),

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
                CALL_SDBUS(sd_bus_message_read(m, "is", &delta, &orientation), ParseError, -1);

                if (TestStrI(orientation, "vertical")) {
                    delta = std::clamp(delta, -1, 1);
                    ToggleProfile(delta);
                }
            }

            return 1;
        }, 0),
        SD_BUS_METHOD("SecondaryActivate", "ii", "", [](sd_bus_message *, void *, sd_bus_error *) { return 1; }, 0),
        SD_BUS_METHOD("ContextMenu", "ii", "", [](sd_bus_message *, void *, sd_bus_error *) { return 1; }, 0),

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

static int GetMenuComplexProperty(sd_bus *, const char *, const char *, const char *property,
                                  sd_bus_message *reply, void *, sd_bus_error *)
{
    if (TestStr(property, "IconThemePath")) {
        CALL_SDBUS(sd_bus_message_append(reply, "as", 0), ReplyError, -1);
        return 1;
    }

    RG_UNREACHABLE();
}

static bool DumpMenuItems(FunctionRef<bool(int, const char *, int)> func)
{
#define ITEM(Id, Label, Check) \
        do { \
            if (!func((Id), (Label), (Check))) \
                return false; \
        } while (false)

    for (Size i = 0; i < profiles.len; i++) {
        const char *name = profiles[i];
        ITEM(100 + i, name, i == profile_idx);
    }

    ITEM(-1, "-", -1);
    ITEM(1, "_Exit", -1);

#undef ITEM

    return true;
}

static void HandleMenuEvent(int id, const char *type)
{
    if (!TestStr(type, "clicked"))
        return;

    if (id == 1) {
        run = false;
    } else if (id >= 100) {
        ApplyProfile(id - 100);
    }
}

static bool RegisterTrayMenu()
{
    static struct {
        unsigned int version = 4;
        const char *status = "normal";
        const char *text_direction = "ltr";
        const char *icon_theme_path = {};
        unsigned int layout = 0;
     } properties;

    static const sd_bus_vtable vtable[] = {
        SD_BUS_VTABLE_START(0),

        SD_BUS_PROPERTY("IconThemePath", "as", GetMenuComplexProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("Version", "u", nullptr, offsetof(decltype(properties), version), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("TextDirection", "s", nullptr, offsetof(decltype(properties), text_direction), SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("Status", "s", nullptr, offsetof(decltype(properties), status), SD_BUS_VTABLE_PROPERTY_CONST),

        SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", [](sd_bus_message *m, void *, sd_bus_error *) {
            sd_bus_message *reply;
            CALL_SDBUS(sd_bus_message_new_method_return(m, &reply), ReplyError, -1);
            RG_DEFER { sd_bus_message_unref(reply); };

            int root;
            CALL_SDBUS(sd_bus_message_read(m, "i", &root), ParseError, -1);

            CALL_SDBUS(sd_bus_message_append(reply, "u", profiles_revision), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "ia{sv}av"), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ia{sv}", 0, 1,
                "children-display", "s", "submenu"
            ), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "v"), ReplyError, -1);
            DumpMenuItems([&](int id, const char *label, int check) {
                if (root != 0)
                    return true;

                CALL_SDBUS(sd_bus_message_append(reply, "v", "(ia{sv}av)", id, 6,
                    "type", "s", TestStr(label, "-") ? "separator" : "standard",
                    "label", "s", TestStr(label, "-") ? "" : label,
                    "enabled", "b", true,
                    "visible", "b", true,
                    "toggle-type", "s", (check >= 0) ? "radio" : "",
                    "toggle-state", "i", check,
                0), ReplyError, false);

                return true;
            });
            CALL_SDBUS(sd_bus_message_close_container(reply), ReplyError, -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), ReplyError, -1);

            return sd_bus_send(nullptr, reply, nullptr);
        }, 0),
        SD_BUS_METHOD("GetGroupProperties", "aias", "a(ia{sv})", [](sd_bus_message *m, void *, sd_bus_error *) {
            sd_bus_message *reply;
            CALL_SDBUS(sd_bus_message_new_method_return(m, &reply), ReplyError, -1);
            RG_DEFER { sd_bus_message_unref(reply); };

            HashSet<int> items;
            CALL_SDBUS(sd_bus_message_enter_container(m, 'a', "i"), ParseError, -1);
            while (sd_bus_message_at_end(m, 0) <= 0) {
                int item;
                CALL_SDBUS(sd_bus_message_read_basic(m, 'i', &item), ParseError, -1);
                items.Set(item);
            }
            CALL_SDBUS(sd_bus_message_exit_container(m), ParseError, -1);

            CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(ia{sv})"), "Failed to prepare sd-bus reply A", -1);
            DumpMenuItems([&](int id, const char *label, int check) {
                if (!items.Find(id))
                    return true;

                CALL_SDBUS(sd_bus_message_append(reply, "(ia{sv})", id, 6,
                    "type", "s", TestStr(label, "-") ? "separator" : "standard",
                    "label", "s", label,
                    "enabled", "b", true,
                    "visible", "b", true,
                    "toggle-type", "s", (check >= 0) ? "radio" : "",
                    "toggle-state", "i", check
                ), "Failed to prepare sd-bus reply X", false);

                return true;
            });
            CALL_SDBUS(sd_bus_message_close_container(reply), "Failed to prepare sd-bus reply B", -1);

            return sd_bus_send(nullptr, reply, nullptr);
        }, 0),
        SD_BUS_METHOD("GetProperty", "is", "v", [](sd_bus_message *, void *, sd_bus_error *) {
            // Not really implemented but nobody seems to care
            return 1;
        }, 0),
        SD_BUS_METHOD("Event", "isvu", "", [](sd_bus_message *m, void *, sd_bus_error *) {
            int item;
            const char *type;
            CALL_SDBUS(sd_bus_message_read(m, "is", &item, &type), ParseError, -1);

            HandleMenuEvent(item, type);

            return 1;
        }, 0),
        SD_BUS_METHOD("EventGroup", "a(isvu)", "ai", [](sd_bus_message *m, void *, sd_bus_error *) {
            CALL_SDBUS(sd_bus_message_enter_container(m, 'a', "(isvu)"), ParseError, -1);
            while (sd_bus_message_at_end(m, 0) <= 0) {
                int item;
                const char *type;
                CALL_SDBUS(sd_bus_message_enter_container(m, 'r', "isvu"), ParseError, -1);
                CALL_SDBUS(sd_bus_message_read(m, "is", &item, &type), ParseError, -1);
                CALL_SDBUS(sd_bus_message_skip(m, "vu"), ParseError, -1);
                CALL_SDBUS(sd_bus_message_exit_container(m), ParseError, -1);

                HandleMenuEvent(item, type);
            }
            CALL_SDBUS(sd_bus_message_exit_container(m), ParseError, -1);

            CALL_SDBUS(sd_bus_reply_method_return(m, "ai", 0), ReplyError, -1);

            return 1;
        }, 0),
        SD_BUS_METHOD("AboutToShow", "i", "b", [](sd_bus_message *m, void *, sd_bus_error *) {
            CALL_SDBUS(sd_bus_reply_method_return(m, "b", false), ReplyError, -1);
            return 1;
        }, 0),
        SD_BUS_METHOD("AboutToShowGroup", "ai", "abab", [](sd_bus_message *m, void *, sd_bus_error *) {
            CALL_SDBUS(sd_bus_reply_method_return(m, "abab", 0, 0), ReplyError, -1);
            return 1;
        }, 0),

        SD_BUS_SIGNAL("ItemsPropertiesUpdated", "a(ia{sv})a(ias)", 0),
        SD_BUS_SIGNAL("LayoutUpdated", "ui", 0),
        SD_BUS_SIGNAL("ItemActivationRequested", "iu", 0),

        SD_BUS_VTABLE_END
    };

    CALL_SDBUS(sd_bus_add_object_vtable(bus_user, nullptr, "/MenuBar", "com.canonical.dbusmenu", vtable, &properties),
               "Failed to create tray icon menu", false);

    return true;
}

static int GetBusTimeout(sd_bus *bus)
{
    uint64_t timeout64;
    CALL_SDBUS(sd_bus_get_timeout(bus, &timeout64), "Failed to get D-Bus connection timeout", -1);

    int timeout = (int)std::min(timeout64 / 1000, (uint64_t)INT_MAX);
    return timeout;
}

static bool HandleServerData()
{
    BlockAllocator temp_alloc;

    const auto read = [&](Span<uint8_t> out_buf) {
        Size received = recv(meestic_fd, out_buf.ptr, out_buf.len, 0);
        if (received < 0) {
            LogError("Failed to receive data from server: %1", strerror(errno));
        }
        return received;
    };

    StreamReader reader(read, "<server>");

    // Don't try to fill buffer, which would block, return as soon as some data is available
    reader.SetLazy(true);

    json_Parser parser(&reader, &temp_alloc);

    parser.ParseObject();
    while (parser.InObject()) {
        Span<const char> key = {};
        parser.ParseKey(&key);

        if (key == "profiles") {
            profiles.Clear();
            profiles_alloc.Reset();

            parser.ParseArray();
            while (parser.InArray()) {
                Span<const char> name = {};
                if (!parser.ParseString(&name))
                    return false;
                profiles.Append(DuplicateString(name, &profiles_alloc).ptr);
            }
        } else if (key == "active") {
            int64_t idx = 0;
            if (!parser.ParseInt(&idx))
                return false;
            if (idx < 0 || idx > profiles.len) {
                LogError("Server sent invalid profile index");
                return false;
            }
            profile_idx = (Size)idx;
        } else if (parser.IsValid()) {
            LogError("Unexpected key '%1'", key);
            return false;
        }
    }
    if (!parser.IsValid())
        return false;

    profiles_revision++;
    CALL_SDBUS(sd_bus_emit_signal(bus_user, "/MenuBar", "com.canonical.dbusmenu",
                                  "LayoutUpdated", "ui", profiles_revision, 0),
               "Failed to emit D-bus signal", false);

    return true;
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Options
    const char *socket_filename = "/run/meestic.sock";

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...]%!0

Options:

    %!..+-S, --socket_file socket%!0       Change control socket
                                   %!D..(default: %2)%!0)",
                FelixTarget, socket_filename);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
        return 0;
    }

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-S", "--socket_file", OptionType::Value)) {
                socket_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    // Open D-Bus connections
    CALL_SDBUS(sd_bus_open_user_with_description(&bus_user, FelixTarget), "Failed to connect to session D-Bus bus", 1);
    RG_DEFER { sd_bus_flush_close_unref(bus_user); };

    // Register the tray icon
    if (!RegisterTrayIcon())
        return 1;
    if (!RegisterTrayMenu())
        return 1;

    // From here on, don't quit abruptly
    WaitForInterrupt(0);

    int status = 0;
    while (run) {
        // Open Meestic socket
        meestic_fd = ConnectToUnixSocket(socket_filename, SOCK_STREAM);
        if (meestic_fd < 0)
            return 1;
        RG_DEFER { close(meestic_fd); };

        // React to main service and D-Bus events
        while (run) {
            struct pollfd pfds[3] = {
                { meestic_fd, POLLIN, 0 },
                { sd_bus_get_fd(bus_user), (short)sd_bus_get_events(bus_user), 0 }
            };

            int timeout = GetBusTimeout(bus_user);
            if (timeout < 0)
                return 1;
            if (timeout == INT_MAX)
                timeout = -1;

            if (poll(pfds, RG_LEN(pfds), timeout) < 0) {
                if (errno == EINTR) {
                    WaitForResult ret = WaitForResult(0);

                    if (ret == WaitForResult::Exit) {
                        LogInfo("Exit requested");
                        run = false;
                    } else if (ret == WaitForResult::Interrupt) {
                        LogInfo("Process interrupted");
                        status = 1;
                        run = false;
                    } else {
                        continue;
                    }
                }

                LogError("Failed to poll I/O descriptors: %1", strerror(errno));
                return 1;
            }

            // Wait and try to reconnect to server when it restarts
            if (pfds[0].revents & (POLLERR | POLLHUP)) {
                LogError("Lost connection to server");

                WaitDelay(3000);
                break;
            }
            if (pfds[0].revents & POLLIN) {
                if (!HandleServerData())
                    break;
            }

            CALL_SDBUS(sd_bus_process(bus_user, nullptr), "Failed to process session D-Bus messages", 1);
        }
    }

    return status;
}

#undef CALL_SDBUS

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }

#endif
