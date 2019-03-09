// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "build_target.hh"

// Temporary struct used until target is created
struct TargetConfig {
    const char *name;
    TargetType type;

    HeapArray<const char *> src_directories;
    HeapArray<const char *> src_filenames;
    HeapArray<const char *> src_ignore;

    const char *c_pch_filename;
    const char *cxx_pch_filename;

    HeapArray<const char *> imports;

    HeapArray<const char *> definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    HASH_TABLE_HANDLER(TargetConfig, name);
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
    DebugAssert(!PathIsAbsolute(src_filename));

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

bool TargetSetBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, len = set.targets.len) { set.targets.RemoveFrom(len); };

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

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

            bool type_specified = false;
            do {
                if (prop.key == "Type") {
                    if (prop.value == "Executable") {
                        target_config.type = TargetType::Executable;
                    } else if (prop.value == "Library") {
                        target_config.type = TargetType::Library;
                    } else {
                        LogError("Unknown target type '%1'", prop.value);
                        valid = false;
                    }

                    type_specified = true;
                } else if (prop.key == "SourceDirectory") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.src_directories);
                } else if (prop.key == "SourceFile") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &set.str_alloc, &target_config.src_filenames);
                } else if (prop.key == "SourceIgnore") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, &set.str_alloc).ptr;
                            target_config.src_ignore.Append(copy);
                        }
                    }
                } else if (prop.key == "ImportFrom") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            Size import_idx = targets_map.FindValue(part, -1);
                            if (import_idx >= 0) {
                                target_config.imports.Append(set.targets[import_idx].name);
                            } else {
                                LogError("Cannot import from unknown target '%1'", part);
                                valid = false;
                            }
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
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (!type_specified) {
                LogError("Type attribute is missing for target '%1'", target_config.name);
                valid = false;
            }

            if (valid && !CreateTarget(&target_config)) {
                valid = false;
            }
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.disable();
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
    DEFER_NC(out_guard, len = set.targets.len) { set.targets.RemoveFrom(len); };

    // Heavy type, so create it directly in HeapArray
    Target *target = set.targets.AppendDefault();

    target->name = target_config->name;
    target->type = target_config->type;
    std::swap(target->definitions, target_config->definitions);
    std::swap(target->include_directories, target_config->include_directories);

    // Gather direct target objects
    {
        HeapArray<const char *> src_filenames;
        for (const char *src_directory: target_config->src_directories) {
            if (!EnumerateDirectoryFiles(src_directory, nullptr, 1024, &temp_alloc, &src_filenames))
                return nullptr;
        }
        src_filenames.Append(target_config->src_filenames);

        for (const char *src_filename: src_filenames) {
            const char *name = SplitStrReverseAny(src_filename, PATH_SEPARATORS).ptr;
            bool ignore = std::any_of(target_config->src_ignore.begin(), target_config->src_ignore.end(),
                                      [&](const char *pattern) { return MatchPathName(name, pattern); });

            if (!ignore) {
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
    }

    // Resolve imported objects and libraries
    {
        std::swap(target->libraries, target_config->libraries);

        for (const char *import_name: target_config->imports) {
            const Target *import;
            {
                Size import_idx = targets_map.FindValue(import_name, -1);
                DebugAssert(import_idx >= 0);

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

    if (target->type == TargetType::Executable) {
#ifdef _WIN32
        target->dest_filename = Fmt(&set.str_alloc, "%1%/%2.exe", output_directory, target->name).ptr;
#else
        target->dest_filename = Fmt(&set.str_alloc, "%1%/%2", output_directory, target->name).ptr;
#endif
    }

    bool appended = targets_map.Append(target_config->name, set.targets.len - 1).second;
    DebugAssert(appended);

    out_guard.disable();
    return target;
}

void TargetSetBuilder::Finish(TargetSet *out_set)
{
    for (const Target &target: set.targets) {
        set.targets_map.Append(&target);
    }

    SwapMemory(&set, out_set, SIZE(set));
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
