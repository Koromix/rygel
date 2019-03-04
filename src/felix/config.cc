// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "config.hh"

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

static void AppendLibraries(Span<const char> str,
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

bool ConfigBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, len = config.targets.len) { config.targets.RemoveFrom(len); };

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

            TargetConfig *target_config = config.targets.AppendDefault();

            target_config->name = DuplicateString(prop.section, &config.str_alloc).ptr;
            if (!targets_set.Append(target_config->name).second) {
                LogError("Duplicate target name '%1'", target_config->name);
                valid = false;
            }
            target_config->type = TargetType::Executable;

            bool type_specified = false;
            do {
                if (prop.key == "Type") {
                    if (prop.value == "Executable") {
                        target_config->type = TargetType::Executable;
                    } else if (prop.value == "Library") {
                        target_config->type = TargetType::Library;
                    } else {
                        LogError("Unknown target type '%1'", prop.value);
                        valid = false;
                    }

                    type_specified = true;
                } else if (prop.key == "SourceDirectory") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &config.str_alloc, &target_config->src_directories);
                } else if (prop.key == "SourceFile") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &config.str_alloc, &target_config->src_filenames);
                } else if (prop.key == "Exclude") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, &config.str_alloc).ptr;
                            target_config->exclusions.Append(copy);
                        }
                    }
                } else if (prop.key == "Precompile_C") {
                    target_config->c_pch_filename = NormalizePath(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "Precompile_CXX") {
                    target_config->cxx_pch_filename = NormalizePath(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "Link_Win32") {
#ifdef _WIN32
                    AppendLibraries(prop.value, &config.str_alloc, &target_config->libraries);
#endif
                } else if (prop.key == "Link_POSIX") {
#ifndef _WIN32
                    AppendLibraries(prop.value, &config.str_alloc, &target->libraries);
#endif
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (!type_specified) {
                LogError("Type attribute is missing for target '%1'", target_config->name);
                valid = false;
            }
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.disable();
    return true;
}

bool ConfigBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (ConfigBuilder::*load_func)(StreamReader &st);
        if (extension == ".ini") {
            load_func = &ConfigBuilder::LoadIni;
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

void ConfigBuilder::Finish(Config *out_config)
{
    for (const TargetConfig &target_config: config.targets) {
        config.targets_map.Append(&target_config);
    }

    SwapMemory(out_config, &config, SIZE(config));
}

bool LoadConfig(Span<const char *const> filenames, Config *out_config)
{
    ConfigBuilder config_builder;
    if (!config_builder.LoadFiles(filenames))
        return false;
    config_builder.Finish(out_config);

    return true;
}
