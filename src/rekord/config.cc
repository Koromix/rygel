// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/libcc/libcc.hh"
#include "vendor/re2/re2/re2.h"
#include "config.hh"

namespace RG {

int rk_ComputeDefaultThreads()
{
    static int threads;

    if (!threads) {
        const char *env = getenv("REKORD_THREADS");

        if (env) {
            char *end_ptr;
            long value = strtol(env, &end_ptr, 10);

            if (end_ptr > env && !end_ptr[0] && value > 0) {
                threads = (int)value;
            } else {
                LogError("REKORD_THREADS must be positive number (ignored)");
                threads = (int)std::thread::hardware_concurrency() * 6;
            }
        } else {
            threads = (int)std::thread::hardware_concurrency() * 6;
        }

        RG_ASSERT(threads > 0);
    }

    return threads;
}

bool rk_Config::Complete(bool require_password)
{
    if (!repository) {
        const char *str = getenv("REKORD_REPOSITORY");
        repository = str ? DuplicateString(str, &str_alloc).ptr : nullptr;
    }

    if (!rk_DecodeURL(repository, this))
        return false;

    if (require_password && !password) {
        const char *str = getenv("REKORD_PASSWORD");

        if (str) {
            password = DuplicateString(str, &str_alloc).ptr;
        } else if (FileIsVt100(stderr)) {
            password = Prompt("Repository password: ", nullptr, "*", &str_alloc);
            if (!password)
                return false;
        }
    }

    switch (type) {
        case rk_DiskType::Local: return true;
        case rk_DiskType::S3: return s3.Complete();
        case rk_DiskType::SFTP: return ssh.Complete();
    }

    RG_UNREACHABLE();
}

bool rk_Config::Validate(bool require_password) const
{
    bool valid = true;

    if (!repository) {
        LogError("Missing repository location");
        valid = false;
    }
    if (require_password && !password) {
        LogError("Missing repository password");
        valid = false;
    }

    switch (type) {
        case rk_DiskType::Local: {} break;
        case rk_DiskType::S3: { valid &= s3.Validate(); } break;
        case rk_DiskType::SFTP: {
            valid &= ssh.Validate();

            if (!ssh.path) {
                LogError("Missing SFTP remote path");
                valid = false;
            }
        } break;
    }

    if (threads < 1) {
        LogError("Threads count cannot be < 1");
        valid = false;
    }

    return valid;
}

static bool LooksLikeURL(Span<const char> str)
{
    bool ret = StartsWith(str, "http://") || StartsWith(str, "https://");
    return ret;
}

static bool LooksLikeSSH(Span<const char> str)
{
    if (StartsWith(str, "ssh://"))
        return true;

    re2::StringPiece sp(str.ptr, (size_t)str.len);
    if (RE2::PartialMatch(sp, "^(?:[a-zA-Z0-9\\._\\-]+@)?[^/]*:"))
        return true;
    if (RE2::PartialMatch(sp, "^[a-zA-Z0-9\\._\\-]*@[^/]:?"))
        return true;

    return false;
}

bool rk_DecodeURL(Span<const char> url, rk_Config *out_config)
{
    if (url == "S3") {
        out_config->repository = "S3";
        out_config->type = rk_DiskType::S3;

        return true;
    } else if (LooksLikeURL(url)) {
        out_config->repository = DuplicateString(url, &out_config->str_alloc).ptr;
        out_config->type = rk_DiskType::S3;

        return s3_DecodeURL(url, &out_config->s3);
    } else if (url == "SFTP") {
        out_config->repository = "SFTP";
        out_config->type = rk_DiskType::SFTP;

        return true;
    } else if (LooksLikeSSH(url)) {
        out_config->repository = DuplicateString(url, &out_config->str_alloc).ptr;
        out_config->type = rk_DiskType::SFTP;

        return ssh_DecodeURL(url, &out_config->ssh);
    } else {
        out_config->repository = DuplicateString(url, &out_config->str_alloc).ptr;
        out_config->type = rk_DiskType::Local;

        return true;
    }
}

bool rk_LoadConfig(StreamReader *st, rk_Config *out_config)
{
    rk_Config config;

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), &config.str_alloc);

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Repository") {
                do {
                    if (prop.key == "Repository") {
                        valid &= rk_DecodeURL(prop.value, &config);
                    } else if (prop.key == "Password") {
                        config.password = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "S3") {
                do {
                    valid &= config.s3.SetProperty(prop.key, prop.value, root_directory);
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "SFTP") {
                do {
                    valid &= config.ssh.SetProperty(prop.key, prop.value, root_directory);
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

    std::swap(*out_config, config);
    return true;
}

bool rk_LoadConfig(const char *filename, rk_Config *out_config)
{
    StreamReader st(filename);
    return rk_LoadConfig(&st, out_config);
}

}
