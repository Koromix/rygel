// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

#define GET_CONNECTION(VarName, ErrorValue) \
    ConnectionData *VarName = ReserveConnection(); \
    if (!(VarName)) \
        return (ErrorValue); \
    RG_DEFER { ReleaseConnection(VarName); };

static thread_local ConnectionData *thread_conn;

class SftpDisk: public rk_Disk {
    struct ListContext {
        Async *tasks;

        std::mutex mutex;
        FunctionRef<bool(const char *path)> func;
    };

    ssh_Config config;

    std::mutex connections_mutex;
    std::condition_variable connections_cv;
    HeapArray<ConnectionData *> connections;

public:
    SftpDisk(const ssh_Config &config, const rk_OpenSettings &settings);
    ~SftpDisk() override;

    bool Init(const char *full_pwd, const char *write_pwd) override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    Size ReadRaw(const char *path, HeapArray<uint8_t> *out_buf) override;

    Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, FunctionRef<bool(const char *path)> func) override;
    StatResult TestRaw(const char *path) override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;

private:
    ConnectionData *ReserveConnection();
    void ReleaseConnection(ConnectionData *conn);

    bool ListRaw(ListContext *ctx, const char *path);
};

SftpDisk::SftpDisk(const ssh_Config &config, const rk_OpenSettings &settings)
    : rk_Disk(settings, std::max(32, 4 * GetCoreCount()))
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
        sftp_free(conn->sftp);

        if (conn->ssh && ssh_is_connected(conn->ssh)) {
            ssh_disconnect(conn->ssh);
        }
        ssh_free(conn->ssh);

        delete conn;
    }
}

bool SftpDisk::Init(const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(url);
    RG_ASSERT(mode == rk_DiskMode::Secure);

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
        if (!make_directory("keys/default"))
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

        for (int i = 0; i < 4096; i++) {
            const char *path = Fmt(&temp_alloc, "%1/blobs/%2", config.path, FmtHex(i).Pad0(-3)).ptr;

            async.Run([=, this]() {
                GET_CONNECTION(conn, false);

                if (sftp_mkdir(conn->sftp, path, 0755) < 0) {
                    LogError("Cannot create directory '%1': %2", path, sftp_GetErrorString(conn->sftp));
                    return false;
                }

                return true;
            });

            directories.Append(path);
        }

        async.Sync();
    }

    if (!InitDefault(full_pwd, write_pwd))
        return false;

    err_guard.Disable();
    return true;
}

Size SftpDisk::ReadRaw(const char *path, Span<uint8_t> out_buf)
{
    GET_CONNECTION(conn, false);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

#if defined(_WIN32)
    int flags = _O_RDONLY;
#else
    int flags = O_RDONLY;
#endif

    sftp_file file = sftp_open(conn->sftp, filename.data, flags, 0);
    if (!file) {
        LogError("Cannot open file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
        return -1;
    }
    RG_DEFER { sftp_close(file); };

    Size read_len = 0;

    while (read_len < out_buf.len) {
        ssize_t bytes = sftp_read(file, out_buf.ptr + read_len, out_buf.len - read_len);
        if (bytes < 0) {
            LogError("Failed to read file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
            return -1;
        }

        read_len += (Size)bytes;

        if (!bytes)
            break;
    }

    return read_len;
}

Size SftpDisk::ReadRaw(const char *path, HeapArray<uint8_t> *out_buf)
{
    GET_CONNECTION(conn, false);

    RG_DEFER_NC(out_guard, len = out_buf->len) { out_buf->RemoveFrom(len); };

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

#if defined(_WIN32)
    int flags = _O_RDONLY;
#else
    int flags = O_RDONLY;
#endif

    sftp_file file = sftp_open(conn->sftp, filename.data, flags, 0);
    if (!file) {
        LogError("Cannot open file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
        return -1;
    }
    RG_DEFER { sftp_close(file); };

    Size read_len = 0;

    for (;;) {
        out_buf->Grow(Mebibytes(1));

        ssize_t bytes = sftp_read(file, out_buf->end(), out_buf->Available());
        if (bytes < 0) {
            LogError("Failed to read file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
            return -1;
        }

        out_buf->len += (Size)bytes;
        read_len += (Size)bytes;

        if (!bytes)
            break;
    }

    out_guard.Disable();
    return read_len;
}

Size SftpDisk::WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    GET_CONNECTION(conn, false);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    Size written_len = 0;

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

            if (file) {
                tmp.len += len;
                break;
            } else if (sftp_get_error(conn->sftp) != SSH_FX_FILE_ALREADY_EXISTS) {
                LogError("Failed to open '%1': %2", tmp.data, sftp_GetErrorString(conn->sftp));
                return -1;
            }
        }

        if (!file) {
            LogError("Failed to create temporary file in '%1'", tmp);
            return -1;
        }
    }
    RG_DEFER_N(file_guard) { sftp_close(file); };
    RG_DEFER_N(tmp_guard) { sftp_unlink(conn->sftp, tmp.data); };

    // Write encrypted content
    bool success = func([&](Span<const uint8_t> buf) {
        written_len += buf.len;

        while (buf.len) {
            ssize_t bytes = sftp_write(file, buf.ptr, (size_t)buf.len);

            if (bytes < 0) {
                LogError("Failed to write to '%1': %2", tmp, sftp_GetErrorString(conn->sftp));
                return false;
            }

            buf.ptr += (Size)bytes;
            buf.len -= (Size)bytes;
        }

        return true;
    });
    if (!success)
        return -1;

    // Finalize file
    if (sftp_fsync(file) < 0) {
        LogError("Failed to flush '%1': %2", tmp, sftp_GetErrorString(conn->sftp));
        return -1;
    }
    sftp_close(file);
    file_guard.Disable();

    // Atomic rename is not supported by older SSH servers, and the error code is unhelpful (Generic failure)
    if (sftp_rename(conn->sftp, tmp.data, filename.data) < 0) {
        bool renamed = false;

        for (int i = 0; i < 20; i++) {
            int rnd = GetRandomInt(50, 100);
            WaitDelay(rnd);

            sftp_unlink(conn->sftp, filename.data);

            if (!sftp_rename(conn->sftp, tmp.data, filename.data)) {
                renamed = true;
                break;
            }
        }

        if (!renamed) {
            LogError("Failed to rename '%1' to '%2': %3", tmp.data, filename.data, sftp_GetErrorString(conn->sftp));
            return -1;
        }
    }

    if (!PutCache(path))
        return -1;

    return written_len;
}

bool SftpDisk::DeleteRaw(const char *path)
{
    GET_CONNECTION(conn, false);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    if (sftp_unlink(conn->sftp, filename.data) < 0 &&
            sftp_get_error(conn->sftp) != SSH_FX_NO_SUCH_FILE) {
        LogError("Failed to delete file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
        return false;
    }

    return true;
}

bool SftpDisk::ListRaw(const char *path, FunctionRef<bool(const char *path)> func)
{
    ListContext ctx = {};
    Async tasks(GetAsync());

    ctx.tasks = &tasks;
    ctx.func = func;

    return ListRaw(&ctx, path);
}

bool SftpDisk::ListRaw(SftpDisk::ListContext *ctx, const char *path)
{
    GET_CONNECTION(conn, false);

    path = path ? path : "";

    LocalArray<char, MaxPathSize + 128> dirname;
    dirname.len = Fmt(dirname.data, "%1/%2", config.path, path).len;

    sftp_dir dir = sftp_opendir(conn->sftp, dirname.data);
    if (!dir) {
        LogError("Failed to enumerate directory '%1': %2", dirname, sftp_GetErrorString(conn->sftp));
        return false;
    }
    RG_DEFER { sftp_closedir(dir); };

    HeapArray<const char *> filenames;
    BlockAllocator temp_alloc;

    Async async(ctx->tasks);

    for (;;) {
        sftp_attributes attr = sftp_readdir(conn->sftp, dir);
        RG_DEFER { sftp_attributes_free(attr); };

        if (!attr) {
            if (sftp_dir_eof(dir))
                break;

            LogError("Failed to enumerate directory '%1': %2", dirname, sftp_GetErrorString(conn->sftp));
            return false;
        }

        if (TestStr(attr->name, ".") || TestStr(attr->name, ".."))
            continue;

        const char *filename = path[0] ? Fmt(&temp_alloc, "%1/%2", path, attr->name).ptr
                                       : DuplicateString(attr->name, &temp_alloc).ptr;

        if (attr->type == SSH_FILEXFER_TYPE_DIRECTORY) {
            if (TestStr(filename, "tmp"))
                continue;
            if (!ListRaw(ctx, filename))
                return false;
        } else {
            filenames.Append(filename);
        }
    }

    if (!async.Sync())
        return false;

    // Give collected paths to callback
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);

        for (const char *filename: filenames) {
            if (!ctx->func(filename))
                return false;
        }
    }

    return true;
}

StatResult SftpDisk::TestRaw(const char *path)
{
    GET_CONNECTION(conn, StatResult::OtherError);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    sftp_attributes attr = sftp_stat(conn->sftp, filename.data);
    RG_DEFER { sftp_attributes_free(attr); };

    if (!attr) {
        int err = sftp_get_error(conn->sftp);

        switch (err) {
            case SSH_FX_NO_SUCH_FILE: return StatResult::MissingPath;
            case SSH_FX_PERMISSION_DENIED: {
                LogError("Failed to stat file '%1': permission denied", filename);
                return StatResult::AccessDenied;
            } break;
            default: {
                LogError("Failed to stat file '%1': %2", filename, sftp_GetErrorString(conn->sftp));
                return StatResult::OtherError;
            } break;
        }
    }
    if (attr->type != SSH_FILEXFER_TYPE_REGULAR) {
        LogError("Path '%1' is not a file", filename);
        return StatResult::OtherError;
    }

    return StatResult::Success;
}

bool SftpDisk::CreateDirectory(const char *path)
{
    GET_CONNECTION(conn, false);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    if (sftp_mkdir(conn->sftp, filename.data, 0755) < 0 &&
            sftp_get_error(conn->sftp) != SSH_FX_FILE_ALREADY_EXISTS) {
        LogError("Failed to create directory '%1': %2", filename, sftp_GetErrorString(conn->sftp));
        return false;
    }

    return true;
}

bool SftpDisk::DeleteDirectory(const char *path)
{
    GET_CONNECTION(conn, false);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    if (sftp_rmdir(conn->sftp, filename.data) < 0 &&
            sftp_get_error(conn->sftp) != SSH_FX_NO_SUCH_FILE) {
        LogError("Failed to delete directory '%1': %2", filename, sftp_GetErrorString(conn->sftp));
        return false;
    }

    return true;
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
    if (--conn->reserved)
        return;

    std::lock_guard<std::mutex> lock(connections_mutex);

    connections.Append(conn);
    connections_cv.notify_one();

    thread_conn = nullptr;
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

#undef GET_CONNECTION

}
