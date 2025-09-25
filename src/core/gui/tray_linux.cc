// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

#if defined(__linux__)

#include "src/core/base/base.hh"
#include "tray.hh"
#include "vendor/basu/src/systemd/sd-bus.h"
#include "vendor/stb/stb_image.h"
#include "vendor/stb/stb_image_resize2.h"

namespace K {

struct IconSet {
    LocalArray<Span<const uint8_t>, 8> pixmaps;
    const char *filename = nullptr;

    BlockAllocator allocator;
};

struct MenuItem {
    const char *label;
    int check;

    std::function<void()> func;
};

static const Vec2<int> IconSizes[] = {
    { 22, 22 },
    { 48, 48 },
    { 64, 64 }
};
static_assert(K_LEN(IconSizes) <= K_LEN(IconSet::pixmaps.data));

#define CALL_SDBUS(Call, Ret) \
    do { \
        int ret = Call; \
         \
        if (ret < 0) { \
            LogError(("D-Bus call failed: %1"), strerror(-ret)); \
            return (Ret); \
        } \
    } while (false)

class LinuxTray: public gui_TrayIcon {
    sd_bus *bus = nullptr;
    char name[512];

    IconSet icons;
    std::function<void()> activate;
    std::function<void()> context;
    std::function<void(int)> scroll;
    BucketArray<MenuItem> items;
    int revision = 0;

public:
    ~LinuxTray();

    bool Init();

    bool SetIcon(Span<const uint8_t> png) override;

    void OnActivation(std::function<void()> func) override;
    void OnContext(std::function<void()> func) override;
    void OnScroll(std::function<void(int)> func) override;

    void AddAction(const char *label, int check, std::function<void()> func) override;
    void AddSeparator() override;
    void ClearMenu() override;

    WaitSource GetWaitSource() override;
    bool ProcessEvents() override;

private:
    bool RegisterIcon();
    static int GetIconComplexProperty(sd_bus *, const char *, const char *, const char *property,
                                      sd_bus_message *reply, void *, sd_bus_error *);
    static int HandleBusMatch(sd_bus_message *m, void *, sd_bus_error *);

    bool RegisterMenu();
    static int GetMenuComplexProperty(sd_bus *, const char *, const char *, const char *property,
                                      sd_bus_message *reply, void *, sd_bus_error *);
    bool DumpMenuItems(FunctionRef<bool(int, const char *, int)> func);
    void HandleMenuEvent(int id, const char *type);
    void UpdateRevision();
};

static thread_local LinuxTray *self;

static bool PrepareIcons(Span<const uint8_t> png, IconSet *out_set)
{
    IconSet set;

    int width, height, channels;
    uint8_t *img = stbi_load_from_memory(png.ptr, png.len, &width, &height, &channels, 4);
    if (!img) {
        LogError("Failed to load PNG tray icon");
        return false;
    }
    K_DEFER { free(img); };

    for (Size i = 0; i < K_LEN(IconSizes); i++) {
        const Vec2<int> size = IconSizes[i];

        Size len = 4 * size.x * size.y;
        Span<uint8_t> pixmap = AllocateSpan<uint8_t>(&set.allocator, len);

        if (!stbir_resize_uint8_linear(img, width, height, 0, pixmap.ptr, size.x, size.y, 0, STBIR_RGBA)) {
            LogError("Failed to resize tray icon");
            return false;
        }

        // Convert from RGBA32 (Big Endian) to ARGB32 (Big Endian)
        for (Size j = 0; j < len; j += 4) {
            uint32_t pixel = BigEndian(*(uint32_t *)(pixmap.ptr + j));

            pixmap[j + 0] = (uint8_t)((pixel >> 0) & 0xFF);
            pixmap[j + 1] = (uint8_t)((pixel >> 24) & 0xFF);
            pixmap[j + 2] = (uint8_t)((pixel >> 16) & 0xFF);
            pixmap[j + 3] = (uint8_t)((pixel >> 8) & 0xFF);
        }

        set.pixmaps.Append(pixmap);
    }

    std::swap(*out_set, set);
    return true;
}

// Expects ARGB32
static void GeneratePNG(Span<const uint8_t> data, int32_t width, int32_t height, HeapArray<uint8_t> *out_png)
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

        chunk.len = BigEndian((uint32_t)K_SIZE(ihdr));
        MemCpy(chunk.type, "IHDR", 4);
        ihdr.width = BigEndian(width);
        ihdr.height = BigEndian(width);
        ihdr.bit_depth = 8;
        ihdr.color_type = 6;
        ihdr.compression = 0;
        ihdr.filter = 0;
        ihdr.interlace = 0;

        out_png->Append(MakeSpan((const uint8_t *)&chunk, K_SIZE(chunk)));
        out_png->Append(MakeSpan((const uint8_t *)&ihdr, K_SIZE(ihdr)));

        // Chunk CRC-32
        Span<const uint8_t> span = MakeSpan(out_png->ptr + chunk_pos + 4, K_SIZE(ihdr) + 4);
        uint32_t crc32 = BigEndian((uint32_t)CRC32(0, span));
        out_png->Append(MakeSpan((const uint8_t *)&crc32, 4));
    }

    // Write image data (IDAT)
    {
        Size chunk_pos = out_png->len;

        ChunkHeader chunk = {};
        chunk.len = 0; // Unknown for now
        MemCpy(chunk.type, "IDAT", 4);
        out_png->Append(MakeSpan((const uint8_t *)&chunk, K_SIZE(chunk)));

        StreamWriter writer(out_png, "<png>", 0, CompressionType::Zlib);
        for (int y = 0; y < height; y++) {
            writer.Write((uint8_t)0); // Scanline filter

            Span<const uint8_t> line = MakeSpan(data.ptr + 4 * y * width, 4 * width);

            for (Size i = 0; i < line.len; i += 4) {
                uint32_t pixel = BigEndian(*(uint32_t *)(line.ptr + i));
                uint8_t rgba32[4];

                rgba32[0] = (uint8_t)((pixel >> 16) & 0xFF);
                rgba32[1] = (uint8_t)((pixel >> 8) & 0xFF);
                rgba32[2] = (uint8_t)((pixel >> 0) & 0xFF);
                rgba32[3] = (uint8_t)((pixel >> 24) & 0xFF);

                writer.Write(rgba32);
            }
        }
        bool success = writer.Close();
        K_ASSERT(success);

        // Fix length
        {
            uint32_t len = BigEndian((uint32_t)(out_png->len - chunk_pos - 8));
            uint32_t *ptr = (uint32_t *)(out_png->ptr + chunk_pos);
            MemCpy(ptr, &len, K_SIZE(len));
        }

        // Chunk CRC-32
        Span<const uint8_t> span = MakeSpan(out_png->ptr + chunk_pos + 4, out_png->len - chunk_pos - 4);
        uint32_t crc32 = BigEndian((uint32_t)CRC32(0, span));
        out_png->Append(MakeSpan((const uint8_t *)&crc32, 4));
    }

    // End image (IEND)
    out_png->Append(footer);
}

LinuxTray::~LinuxTray()
{
    sd_bus_flush_close_unref(bus);

    if (icons.filename) {
        UnlinkFile(icons.filename);
    }
}

bool LinuxTray::Init()
{
    K_ASSERT(!bus);

    if (int ret = sd_bus_open_user_with_description(&bus, FelixTarget); ret < 0) {
        LogError("Failed to connect to session D-Bus bus: %1", strerror(-ret));
        return false;
    }

    if (!RegisterIcon())
        return false;
    if (!RegisterMenu())
        return false;

    return true;
}

bool LinuxTray::SetIcon(Span<const uint8_t> png)
{
    if (!PrepareIcons(png, &icons))
        return false;
    sd_bus_emit_signal(bus, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", "");

    return true;
}

void LinuxTray::OnActivation(std::function<void()> func)
{
    activate = func;
}

void LinuxTray::OnContext(std::function<void()> func)
{
    context = func;
}

void LinuxTray::OnScroll(std::function<void(int)> func)
{
    scroll = func;
}

void LinuxTray::AddAction(const char *label, int check, std::function<void()> func)
{
    K_ASSERT(label);
    K_ASSERT(check <= 1);

    Allocator *alloc;
    MenuItem *item = items.AppendDefault(&alloc);

    // Replace accelerator character '&' with '_'
    {
        Span<char> copy = DuplicateString(label, alloc);

        for (char &c: copy) {
            c = (c == '&') ? '_' : c;
        }

        item->label = copy.ptr;
    }

    item->check = check;
    item->func = func;

    UpdateRevision();
}

void LinuxTray::AddSeparator()
{
    MenuItem *item = items.AppendDefault();
    item->label = "-";

    UpdateRevision();
}

void LinuxTray::ClearMenu()
{
    items.Clear();
    UpdateRevision();
}

static int GetBusTimeout(sd_bus *bus)
{
    uint64_t timeout64;
    CALL_SDBUS(sd_bus_get_timeout(bus, &timeout64), -1);

    int timeout = (int)std::min((timeout64 + 999) / 1000, (uint64_t)INT_MAX);
    return timeout;
}

WaitSource LinuxTray::GetWaitSource()
{
    WaitSource src = { sd_bus_get_fd(bus), (short)sd_bus_get_events(bus), GetBusTimeout(bus) };
    return src;
}

bool LinuxTray::ProcessEvents()
{
    self = this;

    // If this fails the icon will become unresponsive, not much we can do.
    // However this should not happen unless something extreme goes on!
    int ret = sd_bus_process(bus, nullptr);

    if (ret < 0) {
        LogError("Failed to process D-Bus messages: %1", strerror(-ret));
        return false;
    }

    return true;
}

bool LinuxTray::RegisterIcon()
{
    Fmt(name, "org.kde.StatusNotifierItem-%1-1", getpid());

    CALL_SDBUS(sd_bus_request_name(bus, name, 0), false);

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
            if (!self->activate)
                return 1;

            self->activate();
            return 1;
        }, 0),
        SD_BUS_METHOD("Scroll", "is", "", [](sd_bus_message *m, void *, sd_bus_error *) {
            if (!self->scroll)
                return 1;

            static int64_t last_time = -50;
            int64_t now = GetMonotonicTime();

            if (now - last_time >= 50) {
                last_time = now;

                int delta;
                const char *orientation;
                CALL_SDBUS(sd_bus_message_read(m, "is", &delta, &orientation), -1);

                if (TestStrI(orientation, "vertical")) {
                    delta = std::clamp(delta, -1, 1);
                    self->scroll(delta);
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

    CALL_SDBUS(sd_bus_add_object_vtable(bus, nullptr, "/StatusNotifierItem", "org.kde.StatusNotifierItem", vtable, &properties), false);
    CALL_SDBUS(sd_bus_match_signal(bus, nullptr, "org.freedesktop.DBus", nullptr, "org.freedesktop.DBus",
                                   "NameOwnerChanged", HandleBusMatch, nullptr), false);

    // Ignore failure... maybe the watcher is not ready yet?
    sd_bus_call_method(bus, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                      "RegisterStatusNotifierItem", nullptr, nullptr, "s", name);

    return true;
}

int LinuxTray::GetIconComplexProperty(sd_bus *, const char *, const char *, const char *property,
                                      sd_bus_message *reply, void *, sd_bus_error *)
{
    if (TestStr(property, "ToolTip")) {
        const IconSet &icons = self->icons;

        CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "sa(iiay)ss"), -1);
        CALL_SDBUS(sd_bus_message_append(reply, "s", FelixTarget), -1);
        CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(iiay)"), -1);
        for (Size i = 0; i < K_LEN(IconSizes); i++) {
            Vec2<int> size = IconSizes[i];
            Span<const uint8_t> icon = icons.pixmaps[i];

            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "iiay"), -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ii", size.x, size.y), -1);
            CALL_SDBUS(sd_bus_message_append_array(reply, 'y', icon.ptr, (size_t)icon.len), -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), -1);
        }
        CALL_SDBUS(sd_bus_message_close_container(reply), -1);
        CALL_SDBUS(sd_bus_message_append(reply, "ss", FelixTarget, FelixTarget), -1);
        CALL_SDBUS(sd_bus_message_close_container(reply), -1);

        return 1;
    } else if (TestStr(property, "IconName")) {
        IconSet *icons = &self->icons;

        if (!icons->filename) {
            const char *tmp = GetTemporaryDirectory();
            const char *filename = CreateUniqueFile(tmp, "tray", ".png", &icons->allocator);

            if (filename && EnsureDirectoryExists(filename)) {
                HeapArray<uint8_t> png;
                GeneratePNG(icons->pixmaps[0], IconSizes[0].x, IconSizes[0].y, &png);

                if (!WriteFile(png, filename))
                    UnlinkFile(filename);
            }

            icons->filename = filename;
        }

        CALL_SDBUS(sd_bus_message_append(reply, "s", icons->filename), -1);

        return 1;
    } else if (TestStr(property, "IconPixmap")) {
        const IconSet &icons = self->icons;

        CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(iiay)"), -1);
        for (Size i = 0; i < K_LEN(IconSizes); i++) {
            Vec2<int> size = IconSizes[i];
            Span<const uint8_t> icon = icons.pixmaps[i];

            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "iiay"), -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ii", size.x, size.y), -1);
            CALL_SDBUS(sd_bus_message_append_array(reply, 'y', icon.ptr, (size_t)icon.len), -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), -1);
        }
        CALL_SDBUS(sd_bus_message_close_container(reply), -1);

        return 1;
    } else if (TestStr(property, "AttentionIconPixmap")) {
        CALL_SDBUS(sd_bus_message_append(reply, "a(iiay)", 0), -1);
        return 1;
    } else if (TestStr(property, "OverlayIconPixmap")) {
        CALL_SDBUS(sd_bus_message_append(reply, "a(iiay)", 0), -1);
        return 1;
    }

    K_UNREACHABLE();
}

int LinuxTray::HandleBusMatch(sd_bus_message *m, void *, sd_bus_error *)
{
    const char *name;
    CALL_SDBUS(sd_bus_message_read(m, "s", &name), -1);

    if (TestStr(name, "org.kde.StatusNotifierWatcher")) {
        CALL_SDBUS(sd_bus_call_method(self->bus, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                                      "RegisterStatusNotifierItem", nullptr, nullptr, "s", self->name), -1);
        return 1;
    }

    return 1;
}

bool LinuxTray::RegisterMenu()
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
            CALL_SDBUS(sd_bus_message_new_method_return(m, &reply), -1);
            K_DEFER { sd_bus_message_unref(reply); };

            int root;
            CALL_SDBUS(sd_bus_message_read(m, "i", &root), -1);

            CALL_SDBUS(sd_bus_message_append(reply, "u", self->revision), -1);
            CALL_SDBUS(sd_bus_message_open_container(reply, 'r', "ia{sv}av"), -1);
            CALL_SDBUS(sd_bus_message_append(reply, "ia{sv}", 0, 1,
                "children-display", "s", "submenu"
            ), -1);
            CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "v"), -1);
            self->DumpMenuItems([&](int id, const char *label, int check) {
                if (root != 0)
                    return true;

                CALL_SDBUS(sd_bus_message_append(reply, "v", "(ia{sv}av)", id, 6,
                    "type", "s", TestStr(label, "-") ? "separator" : "standard",
                    "label", "s", TestStr(label, "-") ? "" : label,
                    "enabled", "b", true,
                    "visible", "b", true,
                    "toggle-type", "s", (check >= 0) ? "radio" : "",
                    "toggle-state", "i", check,
                0), false);

                return true;
            });
            CALL_SDBUS(sd_bus_message_close_container(reply), -1);
            CALL_SDBUS(sd_bus_message_close_container(reply), -1);

            return sd_bus_send(nullptr, reply, nullptr);
        }, 0),

        SD_BUS_METHOD("GetGroupProperties", "aias", "a(ia{sv})", [](sd_bus_message *m, void *, sd_bus_error *) {
            sd_bus_message *reply;
            CALL_SDBUS(sd_bus_message_new_method_return(m, &reply), -1);
            K_DEFER { sd_bus_message_unref(reply); };

            HashSet<int> items;
            CALL_SDBUS(sd_bus_message_enter_container(m, 'a', "i"), -1);
            while (sd_bus_message_at_end(m, 0) <= 0) {
                int item;
                CALL_SDBUS(sd_bus_message_read_basic(m, 'i', &item), -1);
                items.Set(item);
            }
            CALL_SDBUS(sd_bus_message_exit_container(m), -1);

            CALL_SDBUS(sd_bus_message_open_container(reply, 'a', "(ia{sv})"), -1);
            self->DumpMenuItems([&](int id, const char *label, int check) {
                if (!items.Find(id))
                    return true;

                CALL_SDBUS(sd_bus_message_append(reply, "(ia{sv})", id, 6,
                    "type", "s", TestStr(label, "-") ? "separator" : "standard",
                    "label", "s", label,
                    "enabled", "b", true,
                    "visible", "b", true,
                    "toggle-type", "s", (check >= 0) ? "radio" : "",
                    "toggle-state", "i", check
                ), false);

                return true;
            });
            CALL_SDBUS(sd_bus_message_close_container(reply), -1);

            return sd_bus_send(nullptr, reply, nullptr);
        }, 0),
        SD_BUS_METHOD("GetProperty", "is", "v", [](sd_bus_message *, void *, sd_bus_error *) {
            // Not really implemented but nobody seems to care
            return 1;
        }, 0),
        SD_BUS_METHOD("Event", "isvu", "", [](sd_bus_message *m, void *, sd_bus_error *) {
            int item;
            const char *type;
            CALL_SDBUS(sd_bus_message_read(m, "is", &item, &type), -1);

            self->HandleMenuEvent(item, type);

            return 1;
        }, 0),
        SD_BUS_METHOD("EventGroup", "a(isvu)", "ai", [](sd_bus_message *m, void *, sd_bus_error *) {
            CALL_SDBUS(sd_bus_message_enter_container(m, 'a', "(isvu)"), -1);
            while (sd_bus_message_at_end(m, 0) <= 0) {
                int item;
                const char *type;
                CALL_SDBUS(sd_bus_message_enter_container(m, 'r', "isvu"), -1);
                CALL_SDBUS(sd_bus_message_read(m, "is", &item, &type), -1);
                CALL_SDBUS(sd_bus_message_skip(m, "vu"), -1);
                CALL_SDBUS(sd_bus_message_exit_container(m), -1);

                self->HandleMenuEvent(item, type);
            }
            CALL_SDBUS(sd_bus_message_exit_container(m), -1);

            CALL_SDBUS(sd_bus_reply_method_return(m, "ai", 0), -1);

            return 1;
        }, 0),
        SD_BUS_METHOD("AboutToShow", "i", "b", [](sd_bus_message *m, void *, sd_bus_error *) {
            int item;
            CALL_SDBUS(sd_bus_message_read(m, "i", &item), -1);

            if (!item) {
                if (self->context) {
                    self->context();
                }

                CALL_SDBUS(sd_bus_reply_method_return(m, "b", true), -1);
            } else {
                CALL_SDBUS(sd_bus_reply_method_return(m, "b", false), -1);
            }

            return 1;
        }, 0),
        SD_BUS_METHOD("AboutToShowGroup", "ai", "abab", [](sd_bus_message *m, void *, sd_bus_error *) {
            CALL_SDBUS(sd_bus_reply_method_return(m, "abab", 0, 0), -1);
            return 1;
        }, 0),

        SD_BUS_SIGNAL("ItemsPropertiesUpdated", "a(ia{sv})a(ias)", 0),
        SD_BUS_SIGNAL("LayoutUpdated", "ui", 0),
        SD_BUS_SIGNAL("ItemActivationRequested", "iu", 0),

        SD_BUS_VTABLE_END
    };

    CALL_SDBUS(sd_bus_add_object_vtable(bus, nullptr, "/MenuBar", "com.canonical.dbusmenu", vtable, &properties), false);

    return true;
}

int LinuxTray::GetMenuComplexProperty(sd_bus *, const char *, const char *, const char *property,
                                      sd_bus_message *reply, void *, sd_bus_error *)
{
    if (TestStr(property, "IconThemePath")) {
        CALL_SDBUS(sd_bus_message_append(reply, "as", 0), -1);
        return 1;
    }

    K_UNREACHABLE();
}

bool LinuxTray::DumpMenuItems(FunctionRef<bool(int, const char *, int)> func)
{
    // Keep separate counter because BucketArray iterator is faster than direct indexing
    Size idx = 0;

    for (const MenuItem &item: items) {
        if (!func(idx, item.label, item.check))
            return false;
        idx++;
    }

    return true;
}

void LinuxTray::HandleMenuEvent(int id, const char *type)
{
    if (!TestStr(type, "clicked"))
        return;
    if (id < 0 || id > items.count)
        return;

    const MenuItem &item = items[id];

    if (item.func) {
        // Copy std::function (hopefully this is not expensive) because the handler might destroy it
        // if it uses ClearMenu(), for example. Unlikely but let's play it safe!
        std::function<void()> func = item.func;
        func();
    }
}

void LinuxTray::UpdateRevision()
{
    revision++;
    sd_bus_emit_signal(bus, "/MenuBar", "com.canonical.dbusmenu", "LayoutUpdated", "ui", revision, 0);
}

std::unique_ptr<gui_TrayIcon> gui_CreateTrayIcon(Span<const uint8_t> png)
{
    std::unique_ptr<LinuxTray> tray = std::make_unique<LinuxTray>();

    if (!tray->Init())
        return nullptr;
    if (!tray->SetIcon(png))
        return nullptr;

    return tray;
}

#undef CALL_SDBUS

}

#endif
