// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "disk.hh"
#include "src/core/request/ssh.hh"

#include <fcntl.h>

namespace RG {

static const int MaxPathSize = 4096 - 128;

struct ConnectionData {
    int reserved = 0;

    ssh_session ssh = nullptr;
    sftp_session sftp = nullptr;
};

enum class RunResult {
    Success,
    SpecificError,
    OtherError
};

static thread_local ConnectionData *thread_conn;

class SftpDisk: public rk_Disk {
    ssh_Config config;

    std::mutex connections_mutex;
    std::condition_variable connections_cv;
    HeapArray<ConnectionData *> connections;

public:
    SftpDisk(const ssh_Config &config);
    ~SftpDisk() override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;
    StatResult TestDirectory(const char *path) override;

    Size ReadFile(const char *path, Span<uint8_t> out_buf) override;
    Size ReadFile(const char *path, HeapArray<uint8_t> *out_buf) override;

    rk_WriteResult WriteFile(const char *path, Span<const uint8_t> buf, unsigned int flags) override;
    bool DeleteFile(const char *path) override;

    bool ListFiles(const char *path, FunctionRef<bool(const char *, int64_t)> func) override;
    StatResult TestFile(const char *path, int64_t *out_size = nullptr) override;

private:
    bool RunSafe(const char *action, FunctionRef<RunResult(ConnectionData *conn)> func);

    ConnectionData *ReserveConnection();
    void ReleaseConnection(ConnectionData *conn);
    void DestroyConnection(ConnectionData *conn);
};

static bool IsSftpErrorSpecific(int error)
{
    if (error == SSH_FX_OK)
        return false;
    if (error == SSH_FX_FAILURE)
        return false;
    if (error == SSH_FX_BAD_MESSAGE)
        return false;

    return true;
}

SftpDisk::SftpDisk(const ssh_Config &config)
{
    config.Clone(&this->config);

    if (!this->config.path || !this->config.path[0]) {
        this->config.path = ".";
    }

    // Sanity checks
    if (strlen(this->config.path) > MaxPathSize) {
        LogError("Directory path '%1' is too long", this->config.path);
        return;
    }

    // Connect once to check
    ConnectionData *conn = ReserveConnection();
    if (!conn)
        return;
    ReleaseConnection(conn);

    // We're good!
    if (config.port > 0 && config.port != 22) {
        url = Fmt(&this->config.str_alloc, "sftp://%1@%2:%3/%4", config.username, config.host, config.port, config.path ? config.path : "").ptr;
    } else {
        url = Fmt(&this->config.str_alloc, "sftp://%1@%2/%3", config.username, config.host, config.path ? config.path : "").ptr;
    }
    default_threads = std::min(4 * GetCoreCount(), 64);
}

SftpDisk::~SftpDisk()
{
    for (ConnectionData *conn: connections) {
        DestroyConnection(conn);
    }
}

bool SftpDisk::CreateDirectory(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    bool success = RunSafe("create directory", [&](ConnectionData *conn) {
        if (sftp_mkdir(conn->sftp, filename.data, 0755) < 0) {
            int error = sftp_get_error(conn->sftp);

            if (error == SSH_FX_FILE_ALREADY_EXISTS) {
                return RunResult::Success;
            } else if (IsSftpErrorSpecific(error)) {
                LogError("Failed to create directory '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }

        return RunResult::Success;
    });

    return success;
}

bool SftpDisk::DeleteDirectory(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    bool success = RunSafe("delete directory", [&](ConnectionData *conn) {
        if (sftp_rmdir(conn->sftp, filename.data) < 0) {
            int error = sftp_get_error(conn->sftp);

            if (error == SSH_FX_NO_SUCH_FILE) {
                return RunResult::Success;
            } else if (IsSftpErrorSpecific(error)) {
                LogError("Failed to delete directory '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }

        return RunResult::Success;
    });

    return success;
}

StatResult SftpDisk::TestDirectory(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    StatResult ret = StatResult::Success;

    bool success = RunSafe("stat directory", [&](ConnectionData *conn) {
        sftp_attributes attr = sftp_stat(conn->sftp, filename.data);
        RG_DEFER { sftp_attributes_free(attr); };

        if (!attr) {
            int error = sftp_get_error(conn->sftp);

            switch (error) {
                case SSH_FX_NO_SUCH_FILE: {
                    ret = StatResult::MissingPath;
                    return RunResult::Success;
                } break;

                case SSH_FX_PERMISSION_DENIED: {
                    LogError("Failed to stat file '%1': permission denied", filename);

                    ret = StatResult::AccessDenied;
                    return RunResult::Success;
                } break;

                default: {
                    if (IsSftpErrorSpecific(error)) {
                        LogError("Failed to stat file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                        return RunResult::SpecificError;
                    } else {
                        return RunResult::OtherError;
                    }
                } break;
            }
        }
        if (attr->type != SSH_FILEXFER_TYPE_DIRECTORY) {
            LogError("Path '%1' is not a directory", filename);

            ret = StatResult::OtherError;
            return RunResult::Success;
        }

        return RunResult::Success;
    });
    if (!success)
        return StatResult::OtherError;

    return ret;
}

Size SftpDisk::ReadFile(const char *path, Span<uint8_t> out_buf)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

#if defined(_WIN32)
    int flags = _O_RDONLY;
#else
    int flags = O_RDONLY;
#endif

    Size read_len = 0;

    bool success = RunSafe("read file", [&](ConnectionData *conn) {
        read_len = 0;

        sftp_file file = sftp_open(conn->sftp, filename.data, flags, 0);
        if (!file) {
            int error = sftp_get_error(conn->sftp);

            if (IsSftpErrorSpecific(error)) {
                LogError("Cannot open file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }
        RG_DEFER { sftp_close(file); };

        while (read_len < out_buf.len) {
            ssize_t bytes = sftp_read(file, out_buf.ptr + read_len, out_buf.len - read_len);
            if (bytes < 0) {
                int error = sftp_get_error(conn->sftp);

                if (IsSftpErrorSpecific(error)) {
                    LogError("Failed to read file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                    return RunResult::SpecificError;
                } else {
                    return RunResult::OtherError;
                }
            }

            read_len += (Size)bytes;

            if (!bytes)
                break;
        }

        return RunResult::Success;
    });
    if (!success)
        return -1;

    return read_len;
}

Size SftpDisk::ReadFile(const char *path, HeapArray<uint8_t> *out_buf)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

#if defined(_WIN32)
    int flags = _O_RDONLY;
#else
    int flags = O_RDONLY;
#endif

    Size read_len = 0;

    bool success = RunSafe("read file", [&](ConnectionData *conn) {
        read_len = 0;

        RG_DEFER_NC(out_guard, len = out_buf->len) { out_buf->RemoveFrom(len); };

        sftp_file file = sftp_open(conn->sftp, filename.data, flags, 0);
        if (!file) {
            int error = sftp_get_error(conn->sftp);

            if (IsSftpErrorSpecific(error)) {
                LogError("Cannot open file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }
        RG_DEFER { sftp_close(file); };

        for (;;) {
            out_buf->Grow(Mebibytes(1));

            ssize_t bytes = sftp_read(file, out_buf->end(), out_buf->Available());
            if (bytes < 0) {
                int error = sftp_get_error(conn->sftp);

                if (IsSftpErrorSpecific(error)) {
                    LogError("Failed to read file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                    return RunResult::SpecificError;
                } else {
                    return RunResult::OtherError;
                }
            }

            out_buf->len += (Size)bytes;
            read_len += (Size)bytes;

            if (!bytes)
                break;
        }

        out_guard.Disable();
        return RunResult::Success;
    });
    if (!success)
        return -1;

    return read_len;
}

rk_WriteResult SftpDisk::WriteFile(const char *path, Span<const uint8_t> buf, unsigned int flags)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    bool overwrite = (flags & (int)rk_WriteFlag::Overwrite);

    rk_WriteResult ret = rk_WriteResult::Success;

    bool success = RunSafe("write file", [&](ConnectionData *conn) {
        // Create temporary file
        sftp_file file = nullptr;
        LocalArray<char, MaxPathSize + 128> tmp;
        {
            tmp.len = Fmt(tmp.data, "%1/tmp/", config.path).len;

    #if defined(_WIN32)
            int flags = _O_WRONLY | _O_CREAT | _O_EXCL;
    #else
            int flags = O_WRONLY | O_CREAT | O_EXCL;
    #endif

            for (int i = 0; i < 10; i++) {
                Size len = Fmt(tmp.TakeAvailable(), "%1.tmp", FmtRandom(24)).len;

                file = sftp_open(conn->sftp, tmp.data, flags, 0644);

                if (!file) {
                    int error = sftp_get_error(conn->sftp);

                    if (error == SSH_FX_FILE_ALREADY_EXISTS) {
                        continue;
                    } else if (IsSftpErrorSpecific(error)) {
                        LogError("Failed to open '%1': %2", tmp.data, sftp_GetErrorString(conn->sftp));
                        return RunResult::SpecificError;
                    } else {
                        return RunResult::OtherError;
                    }
                }

                tmp.len += len;
                break;
            }

            if (!file) {
                LogError("Failed to create temporary file in '%1'", tmp);
                return RunResult::SpecificError;
            }
        }

        RG_DEFER_N(tmp_guard) {
            sftp_close(file);
            sftp_unlink(conn->sftp, tmp.data);
        };

        // Write content
        while (buf.len) {
            ssize_t bytes = sftp_write(file, buf.ptr, (size_t)buf.len);

            if (bytes < 0) {
                int error = sftp_get_error(conn->sftp);

                if (IsSftpErrorSpecific(error)) {
                    LogError("Failed to write to '%1': %2", tmp, sftp_GetErrorString(conn->sftp));
                    return RunResult::SpecificError;
                } else {
                    return RunResult::OtherError;
                }
            }

            buf.ptr += (Size)bytes;
            buf.len -= (Size)bytes;
        }

        // Finalize file
        if (sftp_fsync(file) < 0) {
            int error = sftp_get_error(conn->sftp);

            if (IsSftpErrorSpecific(error)) {
                LogError("Failed to flush '%1': %2", tmp, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }

        sftp_close(file);
        file = nullptr;

        if (sftp_rename2(conn->sftp, tmp.data, filename.data, overwrite) < 0) {
            if (!overwrite) {
                // Atomic rename is not supported by older SSH servers, and the error code is unhelpful (Generic failure)...
                // So we need to stat the path to emulate EEXIST.

                sftp_attributes attr = sftp_stat(conn->sftp, filename.data);
                RG_DEFER { sftp_attributes_free(attr); };

                if (attr) {
                    ret = rk_WriteResult::AlreadyExists;
                    return RunResult::Success;
                }
            }

            int error = sftp_get_error(conn->sftp);

            if (IsSftpErrorSpecific(error)) {
                LogError("Failed to rename '%1' to '%2': %3", tmp.data, filename.data, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }

        tmp_guard.Disable();
        return RunResult::Success;
    });
    if (!success)
        return rk_WriteResult::OtherError;

    return ret;
}

bool SftpDisk::DeleteFile(const char *path)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    bool success = RunSafe("delete file", [&](ConnectionData *conn) {
        if (sftp_unlink(conn->sftp, filename.data) < 0) {
            int error = sftp_get_error(conn->sftp);

            if (error == SSH_FX_NO_SUCH_FILE) {
                return RunResult::Success;
            } else if (IsSftpErrorSpecific(error)) {
                LogError("Failed to delete file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }

        return RunResult::Success;
    });

    return success;
}

bool SftpDisk::ListFiles(const char *path, FunctionRef<bool(const char *, int64_t)> func)
{
    BlockAllocator temp_alloc;

    path = path ? path : "";

    const char *dirname0 = path[0] ? Fmt(&temp_alloc, "%1/%2", config.path, path).ptr : config.path;
    Size prefix_len = strlen(config.path);

    HeapArray<const char *> pending_directories;
    pending_directories.Append(dirname0);

    for (Size i = 0; i < pending_directories.len; i++) {
        const char *dirname = pending_directories[i];

        bool success = RunSafe("list directory", [&](ConnectionData *conn) {
            sftp_dir dir = sftp_opendir(conn->sftp, dirname);
            if (!dir) {
                int error = sftp_get_error(conn->sftp);

                if (!i && error == SSH_FX_NO_SUCH_FILE)
                    return RunResult::Success;

                if (IsSftpErrorSpecific(error)) {
                    LogError("Failed to enumerate directory '%1': %2", dirname, sftp_GetErrorString(conn->sftp));
                    return RunResult::SpecificError;
                } else {
                    return RunResult::OtherError;
                }
            }
            RG_DEFER { sftp_closedir(dir); };

            for (;;) {
                sftp_attributes attr = sftp_readdir(conn->sftp, dir);
                RG_DEFER { sftp_attributes_free(attr); };

                if (!attr) {
                    if (sftp_dir_eof(dir))
                        break;

                    int error = sftp_get_error(conn->sftp);

                    if (IsSftpErrorSpecific(error)) {
                        LogError("Failed to enumerate directory '%1': %2", dirname, sftp_GetErrorString(conn->sftp));
                        return RunResult::SpecificError;
                    } else {
                        return RunResult::OtherError;
                    }
                }

                if (TestStr(attr->name, ".") || TestStr(attr->name, ".."))
                    continue;

                const char *filename = Fmt(&temp_alloc, "%1/%2", dirname, attr->name).ptr;
                const char *path = filename + prefix_len + 1;

                if (attr->type == SSH_FILEXFER_TYPE_DIRECTORY) {
                    if (TestStr(path, "tmp"))
                        continue;
                    pending_directories.Append(filename);
                } else {
                    if (!func(path, (int64_t)attr->size))
                        return RunResult::OtherError;
                }
            }

            return RunResult::Success;
        });
        if (!success)
            return false;
    }

    return true;
}

StatResult SftpDisk::TestFile(const char *path, int64_t *out_size)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    StatResult ret = StatResult::Success;

    bool success = RunSafe("stat file", [&](ConnectionData *conn) {
        sftp_attributes attr = sftp_stat(conn->sftp, filename.data);
        RG_DEFER { sftp_attributes_free(attr); };

        if (!attr) {
            int error = sftp_get_error(conn->sftp);

            switch (error) {
                case SSH_FX_NO_SUCH_FILE: {
                    ret = StatResult::MissingPath;
                    return RunResult::Success;
                } break;

                case SSH_FX_PERMISSION_DENIED: {
                    LogError("Failed to stat file '%1': permission denied", filename);

                    ret = StatResult::AccessDenied;
                    return RunResult::Success;
                } break;

                default: {
                    if (IsSftpErrorSpecific(error)) {
                        LogError("Failed to stat file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                        return RunResult::SpecificError;
                    } else {
                        return RunResult::OtherError;
                    }
                } break;
            }
        }
        if (attr->type != SSH_FILEXFER_TYPE_REGULAR) {
            LogError("Path '%1' is not a file", filename);

            ret = StatResult::OtherError;
            return RunResult::Success;
        }

        if (out_size) {
            *out_size = (int64_t)attr->size;
        }
        return RunResult::Success;
    });
    if (!success)
        return StatResult::OtherError;

    return ret;
}

bool SftpDisk::RunSafe(const char *action, FunctionRef<RunResult(ConnectionData *conn)> func)
{
    ConnectionData *conn = ReserveConnection();
    if (!conn)
        return false;
    RG_DEFER { ReleaseConnection(conn); };

    for (int i = 0; i < 9; i++) {
        RunResult ret = func(conn);

        switch (ret) {
            case RunResult::Success: return true;
            case RunResult::SpecificError: return false;

            case RunResult::OtherError: { /* retry */ } break;
        }

        if (conn->reserved == 1) {
            DestroyConnection(conn);
            conn = nullptr;
        }

        int delay = 200 + 100 * (1 << i);
        delay += !!i * GetRandomInt(0, delay / 2);

        WaitDelay(delay);

        if (!conn) {
            conn = ReserveConnection();
            if (!conn)
                return false;
        }
    }

    LogError("Failed to %1: %2", action, sftp_GetErrorString(conn->sftp));
    return false;
}

ConnectionData *SftpDisk::ReserveConnection()
{
    // Deal with reentrancy
    if (thread_conn) {
        thread_conn->reserved++;
        return thread_conn;
    }

    // Reuse existing connection
    {
        std::lock_guard<std::mutex> lock(connections_mutex);

        if (connections.len) {
            ConnectionData *conn = connections.ptr[--connections.len];
            conn->reserved = 1;

            return conn;
        }
    }

    // Try to make a new connection
    ssh_session ssh;
    if (url) {
        PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
        RG_DEFER_N(log_guard) { PopLogFilter(); };

        ssh = ssh_Connect(config);

        if (!ssh) {
            std::unique_lock<std::mutex> lock(connections_mutex);

            while (!connections.len) {
                connections_cv.wait(lock);
            }

            ConnectionData *conn = connections.ptr[--connections.len];

            conn->reserved = 1;
            thread_conn = conn;

            return conn;
        }
    } else {
        ssh = ssh_Connect(config);
        if (!ssh)
            return nullptr;
    }

    ConnectionData *conn = new ConnectionData;
    RG_DEFER_N(err_guard) { delete conn; };

    conn->ssh = ssh;
    conn->sftp = sftp_new(conn->ssh);

    if (!conn->sftp)
        RG_BAD_ALLOC();
    if (sftp_init(conn->sftp) < 0) {
        LogError("Failed to initialize SFTP: %1", ssh_get_error(conn->ssh));
        return nullptr;
    }

    conn->reserved = 1;
    thread_conn = conn;

    err_guard.Disable();
    return conn;
}

void SftpDisk::ReleaseConnection(ConnectionData *conn)
{
    if (!conn)
        return;
    if (--conn->reserved)
        return;

    std::lock_guard<std::mutex> lock(connections_mutex);

    connections.Append(conn);
    connections_cv.notify_one();

    thread_conn = nullptr;
}

void SftpDisk::DestroyConnection(ConnectionData *conn)
{
    if (!conn)
        return;

    sftp_free(conn->sftp);

    if (conn->ssh && ssh_is_connected(conn->ssh)) {
        ssh_disconnect(conn->ssh);
    }
    ssh_free(conn->ssh);

    delete conn;
}

std::unique_ptr<rk_Disk> rk_OpenSftpDisk(const ssh_Config &config)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<SftpDisk>(config);
    if (!disk->GetURL())
        return nullptr;
    return disk;
}

}
