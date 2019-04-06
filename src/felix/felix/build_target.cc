// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "build_target.hh"

namespace RG {

struct FileSet {
    HeapArray<const char *> directories;
    HeapArray<const char *> directories_rec;
    HeapArray<const char *> filenames;
    HeapArray<const char *> ignore;
};

// Temporary struct used until target is created
struct TargetConfig {
    const char *name;
    TargetType type;
    bool enable_by_default;

    FileSet src_file_set;
    const char *c_pch_filename;
    const char *cxx_pch_filename;

    HeapArray<const char *> imports;

    HeapArray<const char *> definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    FileSet pack_file_set;
    const char *pack_options;
    PackLinkType pack_link_type;

    RG_HASH_TABLE_HANDLER(TargetConfig, name);
};

static bool AppendNormalizedPath(Span<const char> path,
                                 Allocator *alloc, HeapArray<const char *> *out_paths)
{
    if (PathIsAbsolute(path)) {
        LogError("Cannot use absolute path '%1'", path);
        return false;
    }

    const char *norm_path = NormalizePath(path, alloc).ptr;
    out_paths->Append(norm_path);

    return true;
}

static void AppendListValues(Span<const char> str,
                             Allocator *alloc, HeapArray<const char *> *out_libraries)
{
    while (str.len) {
        Span<const char> lib = TrimStr(SplitStr(str, ' ', &str));

        if (lib.len) {
            const char *copy = DuplicateString(lib, alloc).ptr;
            out_libraries->Append(copy);
        }
    }
}

static const char *BuildOutputPath(const char *src_filename, const char *output_directory,
                                   const char *suffix, Allocator *alloc)
{
    RG_DEBUG_ASSERT(!PathIsAbsolute(src_filename));

    HeapArray<char> buf;
    buf.allocator = alloc;

    Size offset = Fmt(&buf, "%1%/objects%/", output_directory).len;
    Fmt(&buf, "%1%2", src_filename, suffix);

    // Replace '..' components with '__'
    {
        char *ptr = buf.ptr + offset;

        while ((ptr = strstr(ptr, ".."))) {
            if (IsPathSeparator(ptr[-1]) && (IsPathSeparator(ptr[2]) || !ptr[2])) {
                ptr[0] = '_';
                ptr[1] = '_';
            }

            ptr += 2;
        }
    }

    return buf.Leak().ptr;
}

static bool ResolveFileSet(const FileSet &file_set,
                           Allocator *alloc, HeapArray<const char *> *out_filenames)
{
    RG_DEFER_NC(out_guard, len = out_filenames->len) { out_filenames->RemoveFrom(len); };

    for (const char *directory: file_set.directories) {
        if (!EnumerateFiles(directory, nullptr, 0, 1024, alloc, out_filenames))
            return false;
    }
    for (const char *directory: file_set.directories_rec) {
        if (!EnumerateFiles(directory, nullptr, -1, 1024, alloc, out_filenames))
            return false;
    }
    out_filenames->Append(file_set.filenames);

    out_filenames->RemoveFrom(std::remove_if(out_filenames->begin(), out_filenames->end(),
                                             [&](const char *filename) {
        const char *name = SplitStrReverseAny(filename, RG_PATH_SEPARATORS).ptr;
        bool ignore = std::any_of(file_set.ignore.begin(), file_set.ignore.end(),
                                  [&](const char *pattern) { return MatchPathName(name, pattern); });
        return ignore;
    }) - out_filenames->begin());

    out_guard.Disable();
    return true;
}

bool TargetSetBuilder::LoadIni(StreamReader &st)
{
    RG_DEFER_NC(out_guard, len = set.targets.len) { set.targets.RemoveFrom(len); };

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    RG_DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            TargetConfig target_config = {};

            target_config.name = DuplicateString(prop.section, &set.str_alloc).ptr;
            if (targets_map.Find(target_config.name)) {
                LogError("Duplicate target name '%1'", target_config.name);
                valid = false;
            }
            target_config.type = TargetType::Executable;
            target_config.pack_link_type = PackLinkType::Static;

            // Type property must be specified first
            if (prop.key == "Type") {
                if (prop.value == "Executable") {
                    target_config.type = TargetType::Executable;
                    target_config.enable_by_default = true;
                } else if (prop.value == "Library") {
                    target_config.type = TargetType::Library;
                } else {
                    LogError("Unknown target type '%1'", prop.value);
                    valid = false;
                }
            } else {
                LogError("Property 'Type' must be specified first");
                valid = false;
            }

            bool restricted_platforms = false;
            bool supported_platform = false;
            while (ini.NextInSection(&prop)) {
                if (prop.key == "Type") {
                    LogError("Target type cannot be changed");
                    valid = false;
                } else if (prop.key == "EnableByDefault") {
                    if (prop.value == "1" || prop.value == "On" || prop.value == "Y") {
                        target_config.enable_by_default = true;
                    } else if (prop.value == "0" || prop.value == "Off" || prop.value == "N") {
                        target_config.enable_by_default = false;
                    } else {
                        LogError("Invalid EnableByDefault value '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "Platforms") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            if (part == "Win32") {
#ifdef _WIN32
                                supported_platform = true;
#endif
                            } else if (part == "POSIX") {
#ifndef _WIN32
                                supported_platform = true;
#endif
                            } else {
                                LogError("Unknown platform '%1'", part);
                                valid = false;
                            }
                        }
                    }

                    restricted_platforms = true;
                } else if (prop.key == "SourceDirectory") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.src_file_set.directories);
                } else if (prop.key == "SourceDirectoryRec") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.src_file_set.directories_rec);
                } else if (prop.key == "SourceFile") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.src_file_set.filenames);
                } else if (prop.key == "SourceIgnore") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, &set.str_alloc).ptr;
                            target_config.src_file_set.ignore.Append(copy);
                        }
                    }
                } else if (prop.key == "ImportFrom") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, &set.str_alloc).ptr;
                            target_config.imports.Append(copy);
                        }
                    }
                } else if (prop.key == "IncludeDirectory") {
                    valid &= AppendNormalizedPath(prop.value, &set.str_alloc,
                                                  &target_config.include_directories);
                } else if (prop.key == "Precompile_C") {
                    target_config.c_pch_filename = NormalizePath(prop.value, &set.str_alloc).ptr;
                } else if (prop.key == "Precompile_CXX") {
                    target_config.cxx_pch_filename = NormalizePath(prop.value, &set.str_alloc).ptr;
                } else if (prop.key == "Definitions") {
                    AppendListValues(prop.value, &set.str_alloc, &target_config.definitions);
                } else if (prop.key == "Definitions_Win32") {
#ifdef _WIN32
                    AppendListValues(prop.value, &set.str_alloc, &target_config.definitions);
#endif
                } else if (prop.key == "Definitions_POSIX") {
#ifndef _WIN32
                    AppendListValues(prop.value, &set.str_alloc, &target_config.definitions);
#endif
                } else if (prop.key == "Link") {
                    AppendListValues(prop.value, &set.str_alloc, &target_config.libraries);
                } else if (prop.key == "Link_Win32") {
#ifdef _WIN32
                    AppendListValues(prop.value, &set.str_alloc, &target_config.libraries);
#endif
                } else if (prop.key == "Link_POSIX") {
#ifndef _WIN32
                    AppendListValues(prop.value, &set.str_alloc, &target_config.libraries);
#endif
                } else if (prop.key == "AssetDirectory") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.pack_file_set.directories);
                } else if (prop.key == "AssetDirectoryRec") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.pack_file_set.directories_rec);
                } else if (prop.key == "AssetFile") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.pack_file_set.filenames);

                } else if (prop.key == "AssetIgnore") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, &set.str_alloc).ptr;
                            target_config.pack_file_set.ignore.Append(copy);
                        }
                    }
                } else if (prop.key == "AssetOptions") {
                    target_config.pack_options = DuplicateString(prop.value, &set.str_alloc).ptr;
                } else if (prop.key == "AssetLink") {
                    if (prop.value == "Static") {
                        target_config.pack_link_type = PackLinkType::Static;
                    } else if (prop.value == "Module") {
                        target_config.pack_link_type = PackLinkType::Module;
                    } else if (prop.value == "ModuleIfDebug") {
                        target_config.pack_link_type = PackLinkType::ModuleIfDebug;
                    } else {
                        LogError("Unknown asset link mode '%1'", prop.value);
                        valid = false;
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            }

            supported_platform |= !restricted_platforms;
            if (valid && supported_platform && !CreateTarget(&target_config)) {
                valid = false;
            }
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.Disable();
    return true;
}

bool TargetSetBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (TargetSetBuilder::*load_func)(StreamReader &st);
        if (extension == ".ini") {
            load_func = &TargetSetBuilder::LoadIni;
        } else {
            LogError("Cannot load config from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (st.error) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(st);
    }

    return success;
}

// We steal stuff from TargetConfig so it's not reusable after that
const Target *TargetSetBuilder::CreateTarget(TargetConfig *target_config)
{
    RG_DEFER_NC(out_guard, len = set.targets.len) { set.targets.RemoveFrom(len); };

    // Heavy type, so create it directly in HeapArray
    Target *target = set.targets.AppendDefault();

    // Copy simple values
    target->name = target_config->name;
    target->type = target_config->type;
    target->enable_by_default = target_config->enable_by_default;
    std::swap(target->definitions, target_config->definitions);
    std::swap(target->include_directories, target_config->include_directories);
    target->pack_link_type = target_config->pack_link_type;
    target->pack_options = target_config->pack_options;

    // Gather direct target objects
    {
        HeapArray<const char *> src_filenames;
        if (!ResolveFileSet(target_config->src_file_set, &temp_alloc, &src_filenames))
            return nullptr;

        for (const char *src_filename: src_filenames) {
            ObjectInfo obj = {};

            Span<const char> extension = GetPathExtension(src_filename);
            if (extension == ".c") {
                obj.src_type = SourceType::C_Source;
            } else if (extension == ".cc" || extension == ".cpp") {
                obj.src_type = SourceType::CXX_Source;
            } else {
                continue;
            }

            obj.src_filename = DuplicateString(src_filename, &set.str_alloc).ptr;
            obj.dest_filename = BuildOutputPath(src_filename, output_directory, ".o", &set.str_alloc);

            target->objects.Append(obj);
        }
    }

    // Resolve imported objects and libraries
    {
        std::swap(target->libraries, target_config->libraries);

        for (const char *import_name: target_config->imports) {
            const Target *import;
            {
                Size import_idx = targets_map.FindValue(import_name, -1);
                if (import_idx < 0) {
                    LogError("Cannot import from unknown target '%1'", import_name);
                    return nullptr;
                }

                import = &set.targets[import_idx];
                if (import->type != TargetType::Library) {
                    LogError("Cannot import non-library target '%1'", import->name);
                    return nullptr;
                }
            }

            target->imports.Append(import->imports);
            target->libraries.Append(import->libraries);
            target->objects.Append(import->objects);
        }

        target->imports.Append(target_config->imports);
    }

    // Deduplicate import array, without sorting because ordering matters
    {
        HashSet<const char *> handled_imports;

        Size j = 0;
        for (Size i = 0; i < target->imports.len; i++) {
            target->imports[j] = target->imports[i];
            j += handled_imports.Append(target->imports[i]).second;
        }

        target->imports.RemoveFrom(j);
    }

    // Sort and deduplicate library and object arrays
    std::sort(target->libraries.begin(), target->libraries.end(),
              [](const char *lib1, const char *lib2) {
        return CmpStr(lib1, lib2) < 0;
    });
    target->libraries.RemoveFrom(std::unique(target->libraries.begin(), target->libraries.end(),
                                 [](const char *lib1, const char *lib2) {
        return TestStr(lib1, lib2);
    }) - target->libraries.begin());
    std::sort(target->objects.begin(), target->objects.end(),
              [](const ObjectInfo &obj1, const ObjectInfo &obj2) {
        return CmpStr(obj1.dest_filename, obj2.dest_filename) < 0;
    });
    target->objects.RemoveFrom(std::unique(target->objects.begin(), target->objects.end(),
                                   [](const ObjectInfo &obj1, const ObjectInfo &obj2) {
        return TestStr(obj1.dest_filename, obj2.dest_filename);
    }) - target->objects.begin());

    // PCH files
    if (target_config->c_pch_filename) {
        ObjectInfo obj = {};

        obj.src_type = SourceType::C_Header;
        obj.src_filename = target_config->c_pch_filename;
        obj.dest_filename = BuildOutputPath(target_config->c_pch_filename, output_directory,
                                            ".pch.h", &set.str_alloc);

        target->c_pch_filename = obj.dest_filename;
        target->pch_objects.Append(obj);
    }
    if (target_config->cxx_pch_filename) {
        ObjectInfo obj = {};

        obj.src_type = SourceType::CXX_Header;
        obj.src_filename = target_config->cxx_pch_filename;
        obj.dest_filename = BuildOutputPath(target_config->cxx_pch_filename, output_directory,
                                            ".pch.h", &set.str_alloc);

        target->cxx_pch_filename = obj.dest_filename;
        target->pch_objects.Append(obj);
    }

    // Gather asset filenames
    if (!ResolveFileSet(target_config->pack_file_set, &set.str_alloc, &target->pack_filenames))
        return nullptr;
    if (target->pack_filenames.len) {
        target->pack_obj_filename = Fmt(&set.str_alloc, "%1%/assets%/%2_assets.o",
                                        output_directory, target->name).ptr;
#ifdef _WIN32
        target->pack_module_filename = Fmt(&set.str_alloc, "%1%/%2_assets.dll", output_directory, target->name).ptr;
#else
        target->pack_module_filename = Fmt(&set.str_alloc, "%1%/%2_assets.so", output_directory, target->name).ptr;
#endif
    }

    // Final target output
    if (target->type == TargetType::Executable) {
#ifdef _WIN32
        target->dest_filename = Fmt(&set.str_alloc, "%1%/%2.exe", output_directory, target->name).ptr;
#else
        target->dest_filename = Fmt(&set.str_alloc, "%1%/%2", output_directory, target->name).ptr;
#endif
    }

    bool appended = targets_map.Append(target_config->name, set.targets.len - 1).second;
    RG_DEBUG_ASSERT(appended);

    out_guard.Disable();
    return target;
}

void TargetSetBuilder::Finish(TargetSet *out_set)
{
    for (const Target &target: set.targets) {
        set.targets_map.Append(&target);
    }

    SwapMemory(&set, out_set, RG_SIZE(set));
}

bool LoadTargetSet(Span<const char *const> filenames, const char *output_directory,
                   TargetSet *out_set)
{
    TargetSetBuilder target_set_builder(output_directory);
    if (!target_set_builder.LoadFiles(filenames))
        return false;
    target_set_builder.Finish(out_set);

    return true;
}

}
