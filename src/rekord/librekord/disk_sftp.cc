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
#include "disk.hh"
#include "src/core/libnet/ssh.hh"

#include <fcntl.h>

namespace RG {

// Fix mess caused by windows.h (included by libssh)
#ifdef CreateDirectory
    #undef CreateDirectory
#endif

static const int MaxPathSize = 4096 - 128;

struct ConnectionData {
    int reserved = 0;

    ssh_session ssh = nullptr;
    sftp_session sftp = nullptr;
};

#define GET_CONNECTION(VarName) \
    ConnectionData *VarName = ReserveConnection(); \
    if (!(VarName)) \
        return false; \
    RG_DEFER { ReleaseConnection(VarName); };

static RG_THREAD_LOCAL ConnectionData *thread_conn;

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
    SftpDisk(const ssh_Config &config, int threads);
    ~SftpDisk() override;

    bool Init(const char *full_pwd, const char *write_pwd) override;

    bool CreateDirectory(const char *path) override;
    bool DeleteDirectory(const char *path) override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    Size ReadRaw(const char *path, HeapArray<uint8_t> *out_buf) override;

    Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, FunctionRef<bool(const char *path)> func) override;
    bool TestRaw(const char *path) override;

private:
    ConnectionData *ReserveConnection();
    void ReleaseConnection(ConnectionData *conn);

    bool ListRaw(ListContext *ctx, const char *path);
};

SftpDisk::SftpDisk(const ssh_Config &config, int threads)
{
    if (threads < 0) {
        threads = std::max(32, 4 * GetCoreCount());
    }

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
    this->threads = threads;
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

                    LogError("Failed to enumerate directory '%1': %2", config.path, ssh_get_error(conn->ssh));
                    return false;
                }

                if (TestStr(attr->name, ".") || TestStr(attr->name, ".."))
                    continue;

                LogError("Directory '%1' exists and is not empty", config.path);
                return false;
            }
        } else {
            if (sftp_mkdir(conn->sftp, config.path, 0755) < 0) {
                LogError("Cannot create directory '%1': %2", config.path, ssh_get_error(conn->ssh));
                return false;
            }
        }
    }

    // Init subdirectories
    {
        const auto make_directory = [&](const char *suffix) {
            const char *path = Fmt(&temp_alloc, "%1/%2", config.path, suffix).ptr;

            if (sftp_mkdir(conn->sftp, path, 0755) < 0) {
                LogError("Cannot create directory '%1': %2", path, ssh_get_error(conn->ssh));
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
        Async async(GetThreads());

        for (int i = 0; i < 4096; i++) {
            const char *path = Fmt(&temp_alloc, "%1/blobs/%2", config.path, FmtHex(i).Pad0(-3)).ptr;

            async.Run([=, this]() {
                GET_CONNECTION(conn);

                if (sftp_mkdir(conn->sftp, path, 0755) < 0) {
                    LogError("Cannot create directory '%1': %2", path, ssh_get_error(conn->ssh));
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

bool SftpDisk::CreateDirectory(const char *path)
{
    GET_CONNECTION(conn);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    if (sftp_mkdir(conn->sftp, filename.data, 0755) < 0 &&
            sftp_get_error(conn->sftp) != SSH_FX_FILE_ALREADY_EXISTS) {
        LogError("Failed to create directory '%1': %2", filename, ssh_get_error(conn->ssh));
        return false;
    }

    return true;
}

bool SftpDisk::DeleteDirectory(const char *path)
{
    GET_CONNECTION(conn);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    if (sftp_rmdir(conn->sftp, filename.data) < 0 &&
            sftp_get_error(conn->sftp) != SSH_FX_NO_SUCH_FILE) {
        LogError("Failed to delete directory '%1': %2", filename, ssh_get_error(conn->ssh));
        return false;
    }

    return true;
}

Size SftpDisk::ReadRaw(const char *path, Span<uint8_t> out_buf)
{
    GET_CONNECTION(conn);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

#ifdef _WIN32
    int flags = _O_RDONLY;
#else
    int flags = O_RDONLY;
#endif

    sftp_file file = sftp_open(conn->sftp, filename.data, flags, 0);
    if (!file) {
        LogError("Cannot open file '%1': %2", filename, ssh_get_error(conn->ssh));
        return -1;
    }
    RG_DEFER { sftp_close(file); };

    Size total_len = 0;

    while (total_len < out_buf.len) {
        ssize_t bytes = sftp_read(file, out_buf.ptr + total_len, out_buf.len - total_len);
        if (bytes < 0) {
            LogError("Failed to read file '%1': %2", filename, ssh_get_error(conn->ssh));
            return -1;
        }

        total_len += (Size)bytes;

        if (!bytes)
            break;
    }

    return total_len;
}

Size SftpDisk::ReadRaw(const char *path, HeapArray<uint8_t> *out_buf)
{
    GET_CONNECTION(conn);

    RG_DEFER_NC(out_guard, len = out_buf->len) { out_buf->RemoveFrom(len); };

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

#ifdef _WIN32
    int flags = _O_RDONLY;
#else
    int flags = O_RDONLY;
#endif

    sftp_file file = sftp_open(conn->sftp, filename.data, flags, 0);
    if (!file) {
        LogError("Cannot open file '%1': %2", filename, ssh_get_error(conn->ssh));
        return -1;
    }
    RG_DEFER { sftp_close(file); };

    Size total_len = 0;

    for (;;) {
        out_buf->Grow(Mebibytes(1));

        ssize_t bytes = sftp_read(file, out_buf->end(), out_buf->Available());
        if (bytes < 0) {
            LogError("Failed to read file '%1': %2", filename, ssh_get_error(conn->ssh));
            return -1;
        }

        out_buf->len += (Size)bytes;
        total_len += (Size)bytes;

        if (!bytes)
            break;
    }

    out_guard.Disable();
    return total_len;
}

Size SftpDisk::WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    GET_CONNECTION(conn);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    Size total_len = 0;

    // Create temporary file
    sftp_file file = nullptr;
    LocalArray<char, MaxPathSize + 128> tmp;
    {
        tmp.len = Fmt(tmp.data, "%1/tmp/", config.path).len;

#ifdef _WIN32
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
            } else if (ssh_get_error_code(conn->sftp) != SSH_FX_FILE_ALREADY_EXISTS) {
                LogError("Failed to open '%1': %2", tmp.data, ssh_get_error(conn->ssh));
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
        total_len += buf.len;

        while (buf.len) {
            ssize_t bytes = sftp_write(file, buf.ptr, (size_t)buf.len);

            if (bytes < 0) {
                LogError("Failed to write to '%1': %2", tmp, ssh_get_error(conn->ssh));
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
        LogError("Failed to flush '%1': %2", tmp, ssh_get_error(conn->ssh));
        return -1;
    }
    sftp_close(file);
    file_guard.Disable();

    // Atomic rename
    if (sftp_rename(conn->sftp, tmp.data, filename.data) < 0) {
        sftp_attributes attr = sftp_stat(conn->sftp, filename.data);
        RG_DEFER { sftp_attributes_free(attr); };

        if (!attr) {
            LogError("Failed to rename '%1' to '%2'", tmp.data, filename.data);
            return -1;
        }
    }

    if (!PutCache(path))
        return -1;

    return total_len;
}

bool SftpDisk::DeleteRaw(const char *path)
{
    GET_CONNECTION(conn);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    if (sftp_unlink(conn->sftp, filename.data) < 0 &&
            sftp_get_error(conn->sftp) != SSH_FX_NO_SUCH_FILE) {
        LogError("Failed to delete file '%1': %2", filename, ssh_get_error(conn->ssh));
        return false;
    }

    return true;
}

bool SftpDisk::ListRaw(const char *path, FunctionRef<bool(const char *path)> func)
{
    ListContext ctx = {};
    Async tasks(GetThreads());

    ctx.tasks = &tasks;
    ctx.func = func;

    return ListRaw(&ctx, path);
}

bool SftpDisk::ListRaw(SftpDisk::ListContext *ctx, const char *path)
{
    GET_CONNECTION(conn);

    path = path ? path : "";

    LocalArray<char, MaxPathSize + 128> dirname;
    dirname.len = Fmt(dirname.data, "%1/%2", config.path, path).len;

    sftp_dir dir = sftp_opendir(conn->sftp, dirname.data);
    if (!dir) {
        LogError("Failed to enumerate directory '%1': %2", dirname, ssh_get_error(conn->ssh));
        return false;
    }
    RG_DEFER { sftp_closedir(dir); };

    HeapArray<const char *> temp_paths;
    BlockAllocator temp_alloc;

    Async async(ctx->tasks);

    for (;;) {
        sftp_attributes attr = sftp_readdir(conn->sftp, dir);
        RG_DEFER { sftp_attributes_free(attr); };

        if (!attr) {
            if (sftp_dir_eof(dir))
                break;

            LogError("Failed to enumerate directory '%1': %2", dirname, ssh_get_error(conn->ssh));
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
            temp_paths.Append(filename);
        }
    }

    if (!async.Sync())
        return false;

    // Give collected paths to callback
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);

        for (const char *path: temp_paths) {
            if (!ctx->func(path))
                return false;
        }
    }

    return true;
}

bool SftpDisk::TestRaw(const char *path)
{
    GET_CONNECTION(conn);

    LocalArray<char, MaxPathSize + 128> filename;
    filename.len = Fmt(filename.data, "%1/%2", config.path, path).len;

    sftp_attributes attr = sftp_stat(conn->sftp, filename.data);
    RG_DEFER { sftp_attributes_free(attr); };

    if (!attr && sftp_get_error(conn->sftp) != SSH_FX_NO_SUCH_FILE) {
        LogError("Failed to stat file '%1': %2 (%3)", filename, ssh_get_error(conn->ssh));
    }

    bool exists = !!attr;
    return exists;
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
        throw std::bad_alloc();
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

std::unique_ptr<rk_Disk> rk_OpenSftpDisk(const ssh_Config &config, const char *username, const char *pwd, int threads)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<SftpDisk>(config, threads);

    if (!disk->GetURL())
        return nullptr;
    if (username && !disk->Authenticate(username, pwd))
        return nullptr;

    return disk;
}

#undef GET_CONNECTION

}
