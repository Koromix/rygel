// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "target.hh"

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
    HeapArray<const char *> export_definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    FileSet pack_file_set;
    const char *pack_options;
    PackLinkMode pack_link_mode;

    RG_HASHTABLE_HANDLER(TargetConfig, name);
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

static bool EnumerateSortedFiles(const char *directory, bool recursive,
                                 Allocator *alloc, HeapArray<const char *> *out_filenames)
{
    Size start_idx = out_filenames->len;

    if (!EnumerateFiles(directory, nullptr, recursive ? -1  : 0, 1024, alloc, out_filenames))
            return false;

    std::sort(out_filenames->begin() + start_idx, out_filenames->end(),
              [](const char *filename1, const char *filename2) {
        return CmpStr(filename1, filename2) < 0;
    });

    return true;
}

static bool ResolveFileSet(const FileSet &file_set,
                           Allocator *alloc, HeapArray<const char *> *out_filenames)
{
    RG_DEFER_NC(out_guard, len = out_filenames->len) { out_filenames->RemoveFrom(len); };

    out_filenames->Append(file_set.filenames);
    for (const char *directory: file_set.directories) {
        if (!EnumerateSortedFiles(directory, false, alloc, out_filenames))
            return false;
    }
    for (const char *directory: file_set.directories_rec) {
        if (!EnumerateSortedFiles(directory, true, alloc, out_filenames))
            return false;
    }

    out_filenames->RemoveFrom(std::remove_if(out_filenames->begin(), out_filenames->end(),
                                             [&](const char *filename) {
        return std::any_of(file_set.ignore.begin(), file_set.ignore.end(),
                           [&](const char *pattern) { return MatchPathSpec(filename, pattern); });
    }) - out_filenames->begin());

    out_guard.Disable();
    return true;
}

static bool MatchPlatform(Span<const char> name, bool *out_match)
{
    if (name == "Win32") {
#ifdef _WIN32
        *out_match = true;
#endif
        return true;
    } else if (name == "POSIX") {
#ifndef _WIN32
        *out_match = true;
#endif
        return true;
    } else if (name == "macOS") {
#ifdef __APPLE__
        *out_match = true;
#endif
        return true;
    } else if (name == "Linux") {
#ifdef __linux__
        *out_match = true;
#endif
        return true;
    } else {
        LogError("Unknown platform '%1'", name);
        return false;
    }
}

bool TargetSetBuilder::LoadIni(StreamReader *st)
{
    RG_DEFER_NC(out_guard, len = set.targets.len) { set.targets.RemoveFrom(len); };

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

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
            if (set.targets_map.Find(target_config.name)) {
                LogError("Duplicate target name '%1'", target_config.name);
                valid = false;
            }
            target_config.type = TargetType::Executable;
            target_config.pack_link_mode = PackLinkMode::Static;

            // Type property must be specified first
            if (prop.key == "Type") {
                if (prop.value == "Executable") {
                    target_config.type = TargetType::Executable;
                    target_config.enable_by_default = true;
                } else if (prop.value == "Library") {
                    target_config.type = TargetType::Library;
                } else if (prop.value == "ExternalLibrary") {
                    target_config.type = TargetType::ExternalLibrary;
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
                // These properties do not support platform suffixes
                if (prop.key == "Type") {
                    LogError("Target type cannot be changed");
                    valid = false;
                } else if (prop.key == "Platforms") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            valid &= MatchPlatform(part, &supported_platform);
                        }
                    }

                    restricted_platforms = true;
                } else {
                    Span<const char> platform;
                    prop.key = SplitStr(prop.key, '_', &platform);

                    if (platform.len) {
                        bool use_property = false;
                        valid &= MatchPlatform(platform, &use_property);

                        if (!use_property)
                            continue;
                    }

                    if (prop.key == "EnableByDefault") {
                        valid &= IniParser::ParseBoolValue(prop.value, &target_config.enable_by_default);
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
                    } else if (prop.key == "PrecompileC") {
                        target_config.c_pch_filename = NormalizePath(prop.value, &set.str_alloc).ptr;
                    } else if (prop.key == "PrecompileCXX") {
                        target_config.cxx_pch_filename = NormalizePath(prop.value, &set.str_alloc).ptr;
                    } else if (prop.key == "Definitions") {
                        AppendListValues(prop.value, &set.str_alloc, &target_config.definitions);
                    } else if (prop.key == "ExportDefinitions") {
                        AppendListValues(prop.value, &set.str_alloc, &target_config.export_definitions);
                    } else if (prop.key == "Link") {
                        AppendListValues(prop.value, &set.str_alloc, &target_config.libraries);
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
                            target_config.pack_link_mode = PackLinkMode::Static;
                        } else if (prop.value == "Module") {
                            target_config.pack_link_mode = PackLinkMode::Module;
                        } else if (prop.value == "ModuleIfDebug") {
                            target_config.pack_link_mode = PackLinkMode::ModuleIfDebug;
                        } else {
                            LogError("Unknown asset link mode '%1'", prop.value);
                            valid = false;
                        }
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                }
            }

            supported_platform |= !restricted_platforms;
            if (valid && supported_platform && !CreateTarget(&target_config)) {
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
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

        bool (TargetSetBuilder::*load_func)(StreamReader *st);
        if (extension == ".ini") {
            load_func = &TargetSetBuilder::LoadIni;
        } else {
            LogError("Cannot load config from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (!st.IsValid()) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(&st);
    }

    return success;
}

// We steal stuff from TargetConfig so it's not reusable after that
const TargetInfo *TargetSetBuilder::CreateTarget(TargetConfig *target_config)
{
    RG_DEFER_NC(out_guard, len = set.targets.len) { set.targets.RemoveFrom(len); };

    // Heavy type, so create it directly in HeapArray
    TargetInfo *target = set.targets.AppendDefault();

    // Copy/steal simple values
    target->name = target_config->name;
    target->type = target_config->type;
    target->enable_by_default = target_config->enable_by_default;
    std::swap(target->definitions, target_config->definitions);
    std::swap(target->export_definitions, target_config->export_definitions);
    std::swap(target->include_directories, target_config->include_directories);
    std::swap(target->libraries, target_config->libraries);
    target->pack_link_mode = target_config->pack_link_mode;
    target->pack_options = target_config->pack_options;

    // Resolve imported targets
    {
        HashSet<const char *> handled_imports;

        for (const char *import_name: target_config->imports) {
            const TargetInfo *import = set.targets_map.FindValue(import_name, nullptr);
            if (!import) {
                LogError("Cannot import from unknown target '%1'", import_name);
                return nullptr;
            }
            if (import->type != TargetType::Library && import->type != TargetType::ExternalLibrary) {
                LogError("Cannot import non-library target '%1'", import->name);
                return nullptr;
            }

            for (const TargetInfo *import2: import->imports) {
                if (handled_imports.TrySet(import2->name).second) {
                    target->imports.Append(import2);
                }
            }

            if (handled_imports.TrySet(import->name).second) {
                target->imports.Append(import);
            }
        }

        for (const TargetInfo *import: target->imports) {
            target->definitions.Append(import->export_definitions);
            target->libraries.Append(import->libraries);
            target->sources.Append(import->sources);
        }
    }

    // Gather direct target objects
    {
        HeapArray<const char *> src_filenames;
        if (!ResolveFileSet(target_config->src_file_set, &temp_alloc, &src_filenames))
            return nullptr;

        for (const char *src_filename: src_filenames) {
            Span<const char> extension = GetPathExtension(src_filename);

            const SourceFileInfo *src;
            if (extension == ".c") {
                src = CreateSource(target, src_filename, SourceType::C);
            } else if (extension == ".cc" || extension == ".cpp") {
                src = CreateSource(target, src_filename, SourceType::CXX);
            } else {
                continue;
            }

            target->sources.Append(src);
        }
    }

    // PCH
    if (target_config->c_pch_filename) {
        target->c_pch_src = CreateSource(target, target_config->c_pch_filename, SourceType::C);
    }
    if (target_config->cxx_pch_filename) {
        target->cxx_pch_src = CreateSource(target, target_config->cxx_pch_filename, SourceType::CXX);
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
    std::stable_sort(target->sources.begin(), target->sources.end(),
                     [](const SourceFileInfo *src1, const SourceFileInfo *src2) {
        return CmpStr(src1->filename, src2->filename) < 0;
    });
    target->sources.RemoveFrom(std::unique(target->sources.begin(), target->sources.end(),
                                           [](const SourceFileInfo *src1, const SourceFileInfo *src2) {
        return TestStr(src1->filename, src2->filename);
    }) - target->sources.begin());

    // Gather asset filenames
    if (!ResolveFileSet(target_config->pack_file_set, &set.str_alloc, &target->pack_filenames))
        return nullptr;

    bool appended = set.targets_map.TrySet(target).second;
    RG_ASSERT(appended);

    out_guard.Disable();
    return target;
}

const SourceFileInfo *TargetSetBuilder::CreateSource(const TargetInfo *target, const char *filename, SourceType type)
{
    std::pair<SourceFileInfo **, bool> ret = set.sources_map.TrySetDefault(filename);

    if (ret.second) {
        SourceFileInfo *src = set.sources.AppendDefault();

        src->target = target;
        src->filename = DuplicateString(filename, &set.str_alloc).ptr;
        src->type = type;

        *ret.first = src;
    }

    return *ret.first;
}

void TargetSetBuilder::Finish(TargetSet *out_set)
{
    std::swap(*out_set, set);
}

bool LoadTargetSet(Span<const char *const> filenames, TargetSet *out_set)
{
    TargetSetBuilder target_set_builder;
    if (!target_set_builder.LoadFiles(filenames))
        return false;
    target_set_builder.Finish(out_set);

    return true;
}

}
