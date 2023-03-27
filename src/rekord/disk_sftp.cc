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

namespace RG {

class SftpDisk: public rk_Disk {
    ssh_session ssh = nullptr;
    sftp_session sftp = nullptr;

    int threads;

public:
    SftpDisk(const ssh_Config &config, int threads);
    ~SftpDisk() override;

    bool Init(const char *full_pwd, const char *write_pwd) override;

    int GetThreads() const override;

    Size ReadRaw(const char *path, Span<uint8_t> out_buf) override;
    Size ReadRaw(const char *path, HeapArray<uint8_t> *out_obj) override;

    Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) override;
    bool DeleteRaw(const char *path) override;

    bool ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths) override;

    bool TestSlow(const char *path) override;
    bool TestFast(const char *path) override;
};

SftpDisk::SftpDisk(const ssh_Config &config, int threads)
{
    ssh = ssh_Connect(config);

    sftp = sftp_new(ssh);
    if (!sftp)
        throw std::bad_alloc();

    if (threads > 0) {
        this->threads = threads;
    } else {
        // S3 is slow unless you use parallelism
        this->threads = 64;
    }

    // We're good!
    url = Fmt(&str_alloc, "sftp://%1@%2:%3", config.username, config.host, config.path).ptr;
}

SftpDisk::~SftpDisk()
{
    sftp_free(sftp);

    if (ssh && ssh_is_connected(ssh)) {
        ssh_disconnect(ssh);
    }
    ssh_free(ssh);
}

bool SftpDisk::Init(const char *full_pwd, const char *write_pwd)
{
    RG_ASSERT(mode == rk_DiskMode::Secure);
    return InitKeys(full_pwd, write_pwd);
}

int SftpDisk::GetThreads() const
{
    return threads;
}

Size SftpDisk::ReadRaw(const char *path, Span<uint8_t> out_buf)
{
    RG_UNREACHABLE();
}

Size SftpDisk::ReadRaw(const char *path, HeapArray<uint8_t> *out_obj)
{
    RG_UNREACHABLE();
}

Size SftpDisk::WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func)
{
    RG_UNREACHABLE();
}

bool SftpDisk::DeleteRaw(const char *path)
{
    RG_UNREACHABLE();
}

bool SftpDisk::ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths)
{
    RG_UNREACHABLE();
}

bool SftpDisk::TestSlow(const char *path)
{
    RG_UNREACHABLE();
}

bool SftpDisk::TestFast(const char *path)
{
    RG_UNREACHABLE();
}

std::unique_ptr<rk_Disk> rk_OpenSftpDisk(const ssh_Config &config, const char *username, const char *pwd, int threads)
{
    std::unique_ptr<rk_Disk> disk = std::make_unique<SftpDisk>(config, threads);

    if (!disk->GetURL())
        return nullptr;
    if (pwd && !disk->Open(username, pwd))
        return nullptr;

    return disk;
}

}
