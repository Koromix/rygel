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

// Fix mess caused by windows.h (included by libssh)
#if defined(CreateDirectory)
    #undef CreateDirectory
#endif

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
    struct ListContext {
        Async *tasks;

        std::mutex mutex;
        FunctionRef<bool(const char *, int64_t)> func;
    };

    ssh_Config config;

    std::mutex connections_mutex;
    std::condition_variable connections_cv;
    HeapArray<ConnectionData *> connections;

public:
    SftpDisk(const ssh_Config &config, const rk_OpenSettings &settings);
    ~SftpDisk() override;

    bool Init(Span<const uint8_t> mkey, Span<const rk_UserInfo> users) override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    Size ReadRaw(const char *path, HeapArray<uint8_t> *out_buf) override;

    WriteResult WriteRaw(const char *path, Span<const uint8_t> buf, bool overwrite) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, FunctionRef<bool(const char *, int64_t)> func) override;
    StatResult TestRaw(const char *path, int64_t *out_size = nullptr) override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;

private:
    bool RunSafe(const char *action, FunctionRef<RunResult(ConnectionData *conn)> func);

    ConnectionData *ReserveConnection();
    void ReleaseConnection(ConnectionData *conn);
    void DestroyConnection(ConnectionData *conn);

    bool ListRaw(ListContext *ctx, const char *path);
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

SftpDisk::SftpDisk(const ssh_Config &config, const rk_OpenSettings &settings)
    : rk_Disk(settings, std::min(4 * GetCoreCount(), 64))
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
        url = Fmt(&str_alloc, "sftp://%1@%2:%3/%4", config.username, config.host, config.port, config.path ? config.path : "").ptr;
    } else {
        url = Fmt(&str_alloc, "sftp://%1@%2/%3", config.username, config.host, config.path ? config.path : "").ptr;
    }
}

SftpDisk::~SftpDisk()
{
    for (ConnectionData *conn: connections) {
        DestroyConnection(conn);
    }
}

bool SftpDisk::Init(Span<const uint8_t> mkey, Span<const rk_UserInfo> users)
{
    RG_ASSERT(url);
    RG_ASSERT(!keyset);

    BlockAllocator temp_alloc;

    ConnectionData *conn = ReserveConnection();
    if (!conn)
        return false;
    RG_DEFER { ReleaseConnection(conn); };

    HeapArray<const char *> directories;
    RG_DEFER_N(err_guard) {
        for (Size i = directories.len - 1; i >= 0; i--) {
            const char *dirname = directories[i];
            sftp_rmdir(conn->sftp, dirname);
        }
    };

    // Create main directory
    {
        sftp_dir dir = sftp_opendir(conn->sftp, config.path);

        if (dir) {
            RG_DEFER { sftp_closedir(dir); };

            for (;;) {
                sftp_attributes attr = sftp_readdir(conn->sftp, dir);
                RG_DEFER { sftp_attributes_free(attr); };

                if (!attr) {
                    if (sftp_dir_eof(dir))
                        break;

                    LogError("Failed to enumerate directory '%1': %2", config.path, sftp_GetErrorString(conn->sftp));
                    return false;
                }

                if (TestStr(attr->name, ".") || TestStr(attr->name, ".."))
                    continue;

                LogError("Directory '%1' exists and is not empty", config.path);
                return false;
            }
        } else {
            if (sftp_mkdir(conn->sftp, config.path, 0755) < 0) {
                LogError("Cannot create directory '%1': %2", config.path, sftp_GetErrorString(conn->sftp));
                return false;
            }
        }
    }

    // Init subdirectories
    {
        const auto make_directory = [&](const char *suffix) {
            const char *path = Fmt(&temp_alloc, "%1/%2", config.path, suffix).ptr;

            if (sftp_mkdir(conn->sftp, path, 0755) < 0) {
                LogError("Cannot create directory '%1': %2", path, sftp_GetErrorString(conn->sftp));
                return false;
            }
            directories.Append(path);

            return true;
        };

        if (!make_directory("keys"))
            return false;
        if (!make_directory("tags"))
            return false;
        if (!make_directory("blobs"))
            return false;
        if (!make_directory("tmp"))
            return false;
    }

    // Init blob subdirectories
    {
        Async async(GetAsync());

        for (int i = 0; i < 256; i++) {
            const char *path = Fmt(&temp_alloc, "%1/blobs/%2", config.path, FmtHex(i).Pad0(-2)).ptr;

            async.Run([=, this]() {
                bool success = RunSafe("init directory", [&](ConnectionData *conn) {
                    if (sftp_mkdir(conn->sftp, path, 0755) < 0) {
                        int error = sftp_get_error(conn->sftp);

                        if (IsSftpErrorSpecific(error)) {
                            LogError("Cannot create directory '%1': %2", path, sftp_GetErrorString(conn->sftp));
                            return RunResult::SpecificError;
                        } else {
                            return RunResult::OtherError;
                        }
                    }

                    return RunResult::Success;
                });

                return success;
            });

            directories.Append(path);
        }

        async.Sync();
    }

    if (!InitDefault(mkey, users))
        return false;

    err_guard.Disable();
    return true;
}

Size SftpDisk::ReadRaw(const char *path, Span<uint8_t> out_buf)
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

Size SftpDisk::ReadRaw(const char *path, HeapArray<uint8_t> *out_buf)
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

rk_Disk::WriteResult SftpDisk::WriteRaw(const char *path, Span<const uint8_t> buf, bool overwrite)
{
    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    WriteResult ret = WriteResult::Success;

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
                    ret = WriteResult::AlreadyExists;
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
        return WriteResult::OtherError;

    return ret;
}

bool SftpDisk::DeleteRaw(const char *path)
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

bool SftpDisk::ListRaw(const char *path, FunctionRef<bool(const char *, int64_t)> func)
{
    ListContext ctx = {};
    Async tasks(GetAsync());

    ctx.tasks = &tasks;
    ctx.func = func;

    return ListRaw(&ctx, path);
}

bool SftpDisk::ListRaw(SftpDisk::ListContext *ctx, const char *path)
{
    path = path ? path : "";

    LocalArray<char, MaxPathSize + 128> dirname;
    dirname.len = Fmt(dirname.data, "%1/%2", config.path, path).len;

    struct FileInfo {
        const char *filename;
        int64_t size;
    };

    HeapArray<FileInfo> files;
    BlockAllocator temp_alloc;

    bool success = RunSafe("list directory", [&](ConnectionData *conn) {
        files.RemoveFrom(0);
        temp_alloc.Reset();

        sftp_dir dir = sftp_opendir(conn->sftp, dirname.data);
        if (!dir) {
            int error = sftp_get_error(conn->sftp);

            if (IsSftpErrorSpecific(error)) {
                LogError("Failed to enumerate directory '%1': %2", dirname, sftp_GetErrorString(conn->sftp));
                return RunResult::SpecificError;
            } else {
                return RunResult::OtherError;
            }
        }
        RG_DEFER { sftp_closedir(dir); };

        Async async(ctx->tasks);

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

            const char *filename = path[0] ? Fmt(&temp_alloc, "%1/%2", path, attr->name).ptr
                                           : DuplicateString(attr->name, &temp_alloc).ptr;

            if (attr->type == SSH_FILEXFER_TYPE_DIRECTORY) {
                if (TestStr(filename, "tmp"))
                    continue;
                if (!ListRaw(ctx, filename))
                    return RunResult::SpecificError;
            } else {
                files.Append({ filename, (int64_t)attr->size });
            }
        }

        if (!async.Sync())
            return RunResult::SpecificError;

        return RunResult::Success;
    });
    if (!success)
        return false;

    // Give collected paths to callback
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);

        for (const FileInfo &file: files) {
            if (!ctx->func(file.filename, file.size))
                return false;
        }
    }

    return true;
}

StatResult SftpDisk::TestRaw(const char *path, int64_t *out_size)
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

std::unique_ptr<rk_Disk> rk_OpenSftpDisk(const ssh_Config &config, const char *username, const char *pwd, const rk_OpenSettings &settings)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<SftpDisk>(config, settings);

    if (!disk->GetURL())
        return nullptr;
    if (username && !disk->Authenticate(username, pwd))
        return nullptr;

    return disk;
}

}
