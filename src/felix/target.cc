// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "config.hh"
#include "target.hh"

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

const TargetData *TargetSetBuilder::CreateTarget(const TargetConfig &target_config)
{
    BlockAllocator temp_alloc;

    // Already done?
    {
        Size target_idx = targets_map.FindValue(target_config.name, -1);
        if (target_idx >= 0)
            return &set.targets[target_idx];
    }

    // Gather target objects
    HeapArray<ObjectInfo> objects;
    {
        HeapArray<const char *> src_filenames;
        for (const char *src_directory: target_config.src_directories) {
            if (!EnumerateDirectoryFiles(src_directory, nullptr, 1024, &temp_alloc, &src_filenames))
                return nullptr;
        }
        src_filenames.Append(target_config.src_filenames);

        for (const char *src_filename: src_filenames) {
            const char *name = SplitStrReverseAny(src_filename, PATH_SEPARATORS).ptr;
            bool ignore = std::any_of(target_config.exclusions.begin(), target_config.exclusions.end(),
                                      [&](const char *excl) { return MatchPathName(name, excl); });

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

                objects.Append(obj);
            }
        }
    }

    // Target libraries
    HeapArray<const char *> libraries;
    libraries.Append(target_config.libraries);

    // Resolve imported objects and libraries
    if (resolve_import) {
        for (const char *import_name: target_config.imports) {
            const TargetData *import;
            {
                Size import_idx = targets_map.FindValue(import_name, -1);
                if (import_idx >= 0) {
                    import = &set.targets[import_idx];
                } else {
                    const TargetConfig *import_config = resolve_import(import_name);
                    if (!import_config) {
                        LogError("Cannot find import target '%1'", import_name);
                        return nullptr;
                    }

                    import = CreateTarget(*import_config);
                    if (!import)
                        return nullptr;
                }
            }

            objects.Append(import->objects);
            libraries.Append(import->libraries);
        }
    } else if (target_config.imports.len) {
        LogError("Imports are not enabled");
        return nullptr;
    }

    // Deduplicate object and library arrays
    std::sort(objects.begin(), objects.end(), [](const ObjectInfo &obj1, const ObjectInfo &obj2) {
        return CmpStr(obj1.dest_filename, obj2.dest_filename) < 0;
    });
    objects.RemoveFrom(std::unique(objects.begin(), objects.end(),
                                   [](const ObjectInfo &obj1, const ObjectInfo &obj2) {
        return TestStr(obj1.dest_filename, obj2.dest_filename);
    }) - objects.begin());
    std::sort(libraries.begin(), libraries.end(), [](const char *lib1, const char *lib2) {
        return CmpStr(lib1, lib2) < 0;
    });
    libraries.RemoveFrom(std::unique(libraries.begin(), libraries.end(),
                                   [](const char *lib1, const char *lib2) {
        return TestStr(lib1, lib2);
    }) - libraries.begin());

    // PCH files
    HeapArray<ObjectInfo> pch_objects;
    const char *c_pch_filename = nullptr;
    const char *cxx_pch_filename = nullptr;
    if (target_config.c_pch_filename) {
        ObjectInfo obj = {};

        obj.src_type = SourceType::C_Header;
        obj.src_filename = target_config.c_pch_filename;
        obj.dest_filename = BuildOutputPath(target_config.c_pch_filename, output_directory,
                                            ".pch.h", &set.str_alloc);
        c_pch_filename = obj.dest_filename;

        pch_objects.Append(obj);
    }
    if (target_config.cxx_pch_filename) {
        ObjectInfo obj = {};

        obj.src_type = SourceType::CXX_Header;
        obj.src_filename = target_config.cxx_pch_filename;
        obj.dest_filename = BuildOutputPath(target_config.cxx_pch_filename, output_directory,
                                            ".pch.h", &set.str_alloc);
        cxx_pch_filename = obj.dest_filename;

        pch_objects.Append(obj);
    }

    // Big type, so create it directly in HeapArray
    // Introduce out_guard if things can start to fail after here
    TargetData *target = set.targets.AppendDefault();

    target->name = target_config.name;
    target->type = target_config.type;
    target->include_directories = target_config.include_directories;
    std::swap(target->libraries, libraries);
    std::swap(target->pch_objects, pch_objects);
    target->c_pch_filename = c_pch_filename;
    target->cxx_pch_filename = cxx_pch_filename;

    std::swap(target->objects, objects);
#ifdef _WIN32
    target->dest_filename = Fmt(&set.str_alloc, "%1%/%2.exe", output_directory, target->name).ptr;
#else
    target->dest_filename = Fmt(&set.str_alloc, "%1%/%2", output_directory, target->name).ptr;
#endif

    bool appended = targets_map.Append(target_config.name, set.targets.len - 1).second;
    DebugAssert(appended);

    return target;
}

void TargetSetBuilder::Finish(TargetSet *out_set)
{
    for (const TargetData &target: set.targets) {
        set.targets_map.Append(&target);
    }

    SwapMemory(&set, out_set, SIZE(set));
}
