// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"

namespace K {

bool Config::Complete()
{
    if (!title) {
        const char *str = GetEnv("DROP_TITLE");
        title = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }
    if (!url) {
        const char *str = GetEnv("DROP_URL");
        url = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }

    if (!drop_prefix.changed) {
        const char *str = GetEnv("DROP_PREFIX");

        if (str) {
            drop_prefix.value = DuplicateString(str, &str_alloc).ptr;
            drop_prefix.changed = true;
        }
    }

    if (!drop_quota.changed) {
        const char *str = GetEnv("DROP_QUOTA");

        if (str) {
            if (!ParseSize(str, &drop_quota.value))
                return false;
            drop_quota.changed = true;
        }
    }

    if (!s3.Complete())
        return false;

    if (!smtp.Complete())
        return false;

    return true;
}

bool Config::Validate() const
{
    bool valid = true;

    if (!title || !title[0]) {
        LogError("Missing main title");
        valid = false;
    }
    if (!url || !url[0]) {
        LogError("Missing public URL");
        valid = false;
    }

    valid &= s3.Validate();

    if (drop_prefix.value[0] && !EndsWith(drop_prefix.value, "/")) {
        LogError("S3 drop path must end with '/'");
        valid = false;
    }

    valid &= http.Validate();
    valid &= smtp.Validate();

    if (!internal_auth && !oidc_providers.len) {
        LogError("Cannot disable internal auth if no SSO provider is configured");
        valid = false;
    }

    for (const oidc_Provider &provider: oidc_providers) {
        valid &= provider.Validate();
    }

    return valid;
}

bool LoadConfig(StreamReader *st, Config *out_config)
{
    Config config;

    const char *config_filename = NormalizePath(st->GetFileName(), GetWorkingDirectory(), &config.str_alloc).ptr;
    Span<const char> root_directory = GetPathDirectory(config_filename);
    Span<const char> data_directory = root_directory;

    IniParser ini(st);
    ini.PushLogFilter();
    K_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "General") {
                if (prop.key == "Title") {
                    config.title = DuplicateString(prop.value, &config.str_alloc).ptr;
                } else if (prop.key == "URL") {
                    Span<const char> url = TrimStrRight(prop.value, '/');
                    config.url = DuplicateString(url, &config.str_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "Data") {
                bool first = true;

                do {
                    if (prop.key == "RootDirectory") {
                        if (first) {
                            data_directory = NormalizePath(prop.value, root_directory, &config.str_alloc);
                        } else {
                            LogError("RootDirectory must be first of section");
                            valid = false;
                        }
                    } else if (prop.key == "DatabaseFile") {
                        config.database_filename = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "TempDirectory") {
                        config.tmp_directory = NormalizePath(prop.value, data_directory, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }

                    first = false;
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Drop") {
                if (prop.key == "Quota") {
                    if (ParseSize(prop.value, &config.drop_quota.value)) {
                        config.drop_quota.changed = true;
                    } else {
                        valid = false;
                    }
                } else if (prop.key == "S3Path") {
                    const char *suffix = prop.value.len && !EndsWith(prop.value, "/") ? "/" : "";

                    config.drop_prefix.value = Fmt(&config.str_alloc, "%1%2", prop.value, suffix).ptr;
                    config.drop_prefix.changed = true;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "S3") {
                valid &= config.s3.SetProperty(prop.key, prop.value, root_directory);
            } else if (prop.section == "HTTP") {
                valid &= config.http.SetProperty(prop.key, prop.value, root_directory);
            } else if (prop.section == "SMTP") {
                valid &= config.smtp.SetProperty(prop.key, prop.value, root_directory);
            } else if (prop.section == "Authentication") {
                if (prop.key == "AllowInternal") {
                    valid &= ParseBool(prop.value, &config.internal_auth);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else if (prop.section == "SSO") {
                oidc_Provider *provider = config.oidc_providers.AppendDefault();

                do {
                    valid &= provider->SetProperty(prop.key, prop.value, root_directory);
                } while (ini.NextInSection(&prop));
            } else {
                LogError("Unknown section '%1'", prop.section);
                while (ini.NextInSection(&prop));
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    // Default values
    if (!config.database_filename) {
        config.database_filename = NormalizePath("redropp.db", data_directory, &config.str_alloc).ptr;
    }
    if (!config.tmp_directory) {
        config.tmp_directory = NormalizePath("tmp", data_directory, &config.str_alloc).ptr;
    }

    // Finalize OIDC providers
    {
        for (oidc_Provider &provider: config.oidc_providers) {
            valid &= !provider.url || provider.Finalize();
        }
        if (!valid)
            return false;

        for (const oidc_Provider &provider: config.oidc_providers) {
            if (provider.issuer) {
                config.oidc_map.Set(provider.issuer, &provider);
            }
        }
    }

    std::swap(*out_config, config);
    return true;
}

bool LoadConfig(const char *filename, Config *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

}
