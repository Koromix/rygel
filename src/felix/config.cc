// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "config.hh"

static bool AppendNormalizedPath(Span<const char> path,
                                 Allocator *alloc, HeapArray<const char *> *out_paths)
{
    char *copy = DuplicateString(path, alloc).ptr;

    if (PathIsAbsolute(copy)) {
        LogError("Cannot use absolute path '%1'", copy);
        return false;
    }

#ifdef _WIN32
    for (Size i = 0; i < path.len; i++) {
        copy[i] = (copy[i] == '/') ? '\\' : copy[i];
    }
#endif

    out_paths->Append(copy);
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

    Span<const char> root_dir;
    SplitStrReverseAny(st.filename, PATH_SEPARATORS, &root_dir);

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

            Target *target = config.targets.AppendDefault();

            target->name = DuplicateString(prop.section, &config.str_alloc).ptr;
            if (!targets_set.Append(target->name).second) {
                LogError("Duplicate target name '%1'", target->name);
                valid = false;
            }

            do {
                if (prop.key == "SourceDirectory") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &config.str_alloc, &target->src_directories);
                } else if (prop.key == "SourceFile") {
                    valid &= AppendNormalizedPath(prop.value,
                                                  &config.str_alloc, &target->src_filenames);
                } else if (prop.key == "Exclusions") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStr(prop.value, ' ', &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, &config.str_alloc).ptr;
                            target->exclusions.Append(copy);
                        }
                    }
                } else if (prop.key == "Link_Win32") {
#ifdef _WIN32
                    AppendLibraries(prop.value, &config.str_alloc, &target->libraries);
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
    for (const Target &target: config.targets) {
        config.targets_map.Append(&target);
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
