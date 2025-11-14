// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__APPLE__)

#include "lib/native/base/base.hh"
#include "lib/native/wrap/xml.hh"
#include "compiler.hh"
#include "locate.hh"
#include "macify.hh"
#include <sys/stat.h>

namespace K {

static bool WriteInfoPlist(const char *name, const char *title, const char *icon_filename, const char *dest_filename) {
    static const char *const plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>English</string>
    <key>CFBundleExecutable</key>
    <string>EXECUTABLE</string>
    <key>CFBundleGetInfoString</key>
    <string></string>
    <key>CFBundleIconFile</key>
    <string>ICON</string>
    <key>CFBundleIdentifier</key>
    <string></string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleLongVersionString</key>
    <string></string>
    <key>CFBundleName</key>
    <string>NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string></string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string></string>
    <key>CSResourcesFileMapped</key>
    <true/>
    <key>NSHumanReadableCopyright</key>
    <string></string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <key>NSHighResolutionCapable</key>
    <string>True</string>
</dict>
</plist>
)";

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(plist);
    K_ASSERT(result);

    pugi::xml_node executable_node = doc.select_node("/plist/dict/key[text()='CFBundleExecutable']/following-sibling::string[1]").node();
    pugi::xml_node icon_node = doc.select_node("/plist/dict/key[text()='CFBundleIconFile']/following-sibling::string[1]").node();
    pugi::xml_node title_node = doc.select_node("/plist/dict/key[text()='CFBundleName']/following-sibling::string[1]").node();

    executable_node.text().set(name);
    icon_node.text().set(icon_filename ? SplitStrReverseAny(icon_filename, K_PATH_SEPARATORS).ptr : "");
    title_node.text().set(title ? title : name);

    class StaticWriter: public pugi::xml_writer {
        StreamWriter writer;

    public:
        StaticWriter(const char *filename) : writer(filename) {}

        void write(const void *buf, size_t len) override { writer.Write((const uint8_t *)buf, (Size)len); }
        bool Close() { return writer.Close(); }
    };

    // Export XML file
    {
        StaticWriter writer(dest_filename);
        doc.save(writer);

        if (!writer.Close())
            return false;
    }

    return true;
}

static bool CopyFile(const char *src_filename, const char *dest_filename)
{
    StreamReader reader(src_filename);
    StreamWriter writer(dest_filename);

    if (!SpliceStream(&reader, -1, &writer))
        return false;
    if (!writer.Close())
        return false;

    return true;
}

static bool CopyRecursive(const char *src_directory, const char *dest_directory, int max_depth, Allocator *alloc)
{
    if (!MakeDirectory(dest_directory, false))
        return false;

    EnumResult ret = EnumerateDirectory(src_directory, nullptr, -1, [&](const char *basename, FileType file_type) {
        const char *filename = Fmt(alloc, "%1%/%2", src_directory, basename).ptr;

        if (file_type == FileType::Directory) {
            if (max_depth) {
                const char *dest = Fmt(alloc, "%1%/%2", dest_directory, basename).ptr;
                return CopyRecursive(filename, dest, max_depth - 1, alloc);
            }
        } else if (file_type == FileType::File) {
            const char *dest = Fmt(alloc, "%1%/%2", dest_directory, basename).ptr;
            return CopyFile(filename, dest);
        }

        LogDebug("Ignoring file type '%1'", FileTypeNames[(int)file_type]);
        return true;
    });

    return (ret == EnumResult::Success);
}

static bool UnlinkRecursive(const char *root_directory)
{
    BlockAllocator temp_alloc;

    HeapArray<const char *> directories;
    directories.Append(root_directory);

    bool complete = true;

    // If it's only a file or a link...
    {
        FileInfo file_info;
        if (StatFile(root_directory, &file_info) != StatResult::Success)
            return false;

        if (file_info.type == FileType::File || file_info.type == FileType::Link)
            return UnlinkFile(root_directory);
    }

    for (Size i = 0; i < directories.len; i++) {
        const char *directory = directories[i];

        EnumResult ret = EnumerateDirectory(directory, nullptr, -1, [&](const char *basename, FileType file_type) {
            const char *filename = Fmt(&temp_alloc, "%1%/%2", directory, basename).ptr;

            if (file_type == FileType::Directory) {
                directories.Append(filename);
            } else {
                complete &= UnlinkFile(filename);
            }

            return true;
        });
        complete &= (ret == EnumResult::Success);
    }

    for (Size i = directories.len - 1; i >= 0; i--) {
        const char *directory = directories[i];
        complete &= UnlinkDirectory(directory);
    }

    return complete;
}

bool BundleMacBinary(const char *binary_filename, const char *output_dir, const MacBundleSettings &settings)
{
    BlockAllocator temp_alloc;

    K_ASSERT(binary_filename);
    K_ASSERT(output_dir);

    std::unique_ptr<const Compiler> compiler = PrepareCompiler({});
    if (!compiler)
        return false;

    const QtInfo *qt = FindQtSdk(compiler.get());
    if (!qt)
        return false;

    if (TestFile(output_dir)) {
        if (settings.force) {
            if (!UnlinkRecursive(output_dir))
                return false;
        } else {
            LogError("Bundle '%1' already exists", output_dir);
            return false;
        }
    }

    if (!MakeDirectory(output_dir))
        return false;

    K_DEFER_N(root_guard) { UnlinkRecursive(output_dir); };

    // Create directories
    {
        const auto make_directory = [&](const char *basename) {
            const char *dirname = Fmt(&temp_alloc, "%1%/%2", output_dir, basename).ptr;
            return MakeDirectory(dirname);
        };

        if (!make_directory("Contents"))
            return false;
        if (!make_directory("Contents/Frameworks"))
            return false;
        if (!make_directory("Contents/MacOs"))
            return false;
        if (!make_directory("Contents/Resources"))
            return false;
    }

    const char *name = SplitStrReverseAny(binary_filename, K_PATH_SEPARATORS).ptr;
    const char *target_binary = Fmt(&temp_alloc, "%1%/Contents%/MacOs%/%2", output_dir, name).ptr;
    const char *plist_filename = Fmt(&temp_alloc, "%1%/Contents%/Info.plist", output_dir).ptr;

    // Copy binary to bundle
    if (!CopyFile(binary_filename, target_binary))
        return false;
    chmod(target_binary, 0755);

    // Copy icon (if any)
    if (settings.icon_filename) {
        const char *dest_icon = Fmt(&temp_alloc, "%1%/Contents%/Resources%/%2",
                                    output_dir, SplitStrReverseAny(settings.icon_filename, K_PATH_SEPARATORS)).ptr;
        if (!CopyFile(settings.icon_filename, dest_icon))
            return false;
    }

    // Write metadata file
    if (!WriteInfoPlist(name, settings.title, settings.icon_filename, plist_filename))
        return false;

    // Run macdeployqt file
    {
        const char *cmd_line = Fmt(&temp_alloc, "\"%1\" \"%2\"", qt->macdeployqt, output_dir).ptr;

        HeapArray<char> output;
        if (!ReadCommandOutput(cmd_line, &output)) {
            LogError("Failed to use macdeployqt: %1", output.As());
            return false;
        }
    }

    root_guard.Disable();
    return true;
}

}

#endif
