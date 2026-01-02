// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"
#include "vendor/lz4/lib/lz4hc.h"

namespace K {

bool rk_Config::Complete()
{
    if (!url) {
        url = GetEnv("REKKORD_REPOSITORY");
    }

    if (url && !rk_DecodeURL(url, this))
        return false;

    if (!key_filename) {
        key_filename = GetEnv("REKKORD_KEYFILE");
    }

    if (!api_key) {
        api_key = GetEnv("REKKORD_AGENT_KEY");
    }

    switch (type) {
        case rk_DiskType::Local: return true;
        case rk_DiskType::S3: return s3.remote.Complete();
        case rk_DiskType::SFTP: return ssh.Complete();
    }

    K_UNREACHABLE();
}

bool rk_Config::Validate(unsigned int flags) const
{
    bool valid = true;

    if (!url) {
        LogError("Missing repository location");
        valid = false;
    }

    if (flags & (int)rk_ConfigFlag::RequireAuth) {
        if (!key_filename) {
            LogError("Missing repository key file");
            valid = false;
        }
    }

    if (flags & (int)rk_ConfigFlag::RequireAgent) {
        if (!connect_url) {
            LogError("Missing connect URL for agent");
            valid = false;
        }
        if (!api_key) {
            LogError("Missing connect API key");
            valid = false;
        }
    } else {
        if (!api_key) {
            LogError("Missing connect API key");
            valid = false;
        }
    }

    if (retain) {
        if (safety) {
            if (retain < rk_MinimalRetention) {
                LogError("Retain duration is too low, disable DurationSafety to override");
                valid = false;
            } else if (retain > rk_MaximalRetention) {
                LogError("Retain duration is too high, disable DurationSafety to override");
                valid = false;
            }
        }

        if (type != rk_DiskType::S3) {
            LogError("Retain locks are only supported with S3 providers");
            valid = false;
        }
    }

    switch (type) {
        case rk_DiskType::Local: {} break;
        case rk_DiskType::S3: { valid &= s3.remote.Validate(); } break;
        case rk_DiskType::SFTP: { valid &= ssh.Validate(); } break;
    }

    return valid;
}

static bool LooksLikeS3(Span<const char> str)
{
    if (StartsWith(str, "s3:")) {
        str = str.Take(3, str.len - 3);
    }

    bool ret = StartsWith(str, "http://") || StartsWith(str, "https://");
    return ret;
}

static bool LooksLikeUserName(Span<const char> str)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '_' || c == '.' || c == '-'; };

    if (!str.len)
        return false;
    if (!std::all_of(str.begin(), str.end(), test_char))
        return false;

    return true;
}

static bool LooksLikeHost(Span<const char> str)
{
    if (!str.len)
        return false;
    if (memchr(str.ptr, '/', (size_t)str.len))
        return false;

    return true;
}

static bool LooksLikeSSH(Span<const char> str)
{
    if (StartsWith(str, "ssh://") || StartsWith(str, "sftp://"))
        return true;

    // Test for user@host pattern
    {
        Span<const char> remain = str;

        Span<const char> username = SplitStr(remain, '@', &remain);
        Span<const char> host = SplitStr(remain, ':', &remain);
        Span<const char> path = remain;

        if (host.ptr > username.end() && path.ptr > host.end() &&
                LooksLikeUserName(username) && LooksLikeHost(host))
            return true;
    }

    return false;
}

bool rk_DecodeURL(Span<const char> url, rk_Config *out_config)
{
    if (url == "S3") {
        out_config->url = "S3";
        out_config->type = rk_DiskType::S3;

        return true;
    } else if (LooksLikeS3(url)) {
        out_config->url = DuplicateString(url, &out_config->str_alloc).ptr;
        out_config->type = rk_DiskType::S3;

        return s3_DecodeURL(url, &out_config->s3.remote);
    } else if (url == "SFTP") {
        out_config->url = "SFTP";
        out_config->type = rk_DiskType::SFTP;

        return true;
    } else if (LooksLikeSSH(url)) {
        out_config->url = DuplicateString(url, &out_config->str_alloc).ptr;
        out_config->type = rk_DiskType::SFTP;

        return ssh_DecodeURL(url, &out_config->ssh);
    } else {
        out_config->url = DuplicateString(url, &out_config->str_alloc).ptr;
        out_config->type = rk_DiskType::Local;

        return true;
    }
}

const char *rk_MakeURL(const rk_Config &config, Allocator *alloc)
{
    K_ASSERT(config.url);

    switch (config.type) {
        case rk_DiskType::Local: return NormalizePath(config.url, alloc).ptr;
        case rk_DiskType::S3: return s3_MakeURL(config.s3.remote, alloc);
        case rk_DiskType::SFTP: return ssh_MakeURL(config.ssh, alloc);
    }

    K_UNREACHABLE();
}

bool rk_LoadConfig(StreamReader *st, rk_Config *out_config)
{
    rk_Config config;

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), &config.str_alloc);

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Repository") {
                if (prop.key == "URL") {
                    if (rk_DecodeURL(prop.value, &config)) {
                        if (config.type == rk_DiskType::Local && !PathIsAbsolute(config.url)) {
                            // Fix local repository URLs to be relative to the INI file directory
                            config.url = NormalizePath(config.url, root_directory, &config.str_alloc).ptr;
                        }
                    } else {
                        valid = false;
                    }
                } else if (prop.key == "KeyFile") {
                    config.key_filename = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Settings") {
                if (prop.key == "Threads") {
                    if (ParseInt(prop.value, &config.threads)) {
                        if (config.threads < 1) {
                            LogError("Threads count cannot be < 1");
                            valid = false;
                        }
                    } else {
                        valid = false;
                    }
                } else if (prop.key == "CompressionLevel") {
                    valid &= ParseInt(prop.value, &config.compression_level);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Protection") {
                if (prop.key == "TestWrites") {
                    valid &= ParseBool(prop.value, &config.ocd);
                } else if (prop.key == "RetainDuration") {
                    if (prop.value == "Disabled") {
                        config.retain = 0;
                    } else if (ParseDuration(prop.value, &config.retain)) {
                        if (config.retain < 0) {
                            LogError("Retain duration cannot be negative");
                            valid = false;
                        }
                    } else {
                        valid = false;
                    }
                } else if (prop.key == "DurationSafety") {
                    valid &= ParseBool(prop.value, &config.safety);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Connect") {
                if (prop.key == "URL") {
                    config.connect_url = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "ApiKey") {
                    config.api_key = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "CheckPeriod") {
                    if (ParseDuration(prop.value, &config.agent_period)) {
                        if (config.agent_period <= 0) {
                            LogError("Check period cannot be negative or zero");
                            valid = false;
                        }
                    } else {
                        valid = false;
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "S3") {
                if (prop.key == "LockMode") {
                    if (!OptionToEnumI(s3_LockModeNames, prop.value, &config.s3.lock)) {
                        LogError("Invalid lock mode '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "ChecksumType") {
                    if (!OptionToEnumI(rk_ChecksumTypeNames, prop.value, &config.s3.checksum)) {
                        LogError("Invalid checksum type '%1'", prop.value);
                        valid = false;
                    }
                } else {
                    valid &= config.s3.remote.SetProperty(prop.key, prop.value, root_directory);
                }
            } else if (prop.section == "SSH" || prop.section == "SFTP") {
                valid &= config.ssh.SetProperty(prop.key, prop.value, root_directory);
            } else {
                LogError("Unknown section '%1'", prop.section);
                while (ini.NextInSection(&prop));
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    std::swap(*out_config, config);
    return true;
}

bool rk_LoadConfig(const char *filename, rk_Config *out_config)
{
    StreamReader st(filename);
    return rk_LoadConfig(&st, out_config);
}

}
