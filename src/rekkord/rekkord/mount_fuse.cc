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

#include "src/core/base/base.hh"
#include "rekkord.hh"

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)

#include "vendor/libfuse/include/fuse.h"

namespace RG {

struct CacheEntry {
    CacheEntry *parent;

    const char *name;
    rk_Hash hash;
    struct stat st;

    struct {
        std::mutex mutex;
        bool ready;

        HeapArray<CacheEntry> children;
        BlockAllocator str_alloc;
    } directory;

    struct {
        bool ready;
        const char *target;
    } link;

    mutable std::atomic_int refcount { 0 };

    void Unref() const { refcount--; }
};

static std::unique_ptr<rk_Disk> disk = nullptr;
static CacheEntry root = {};

static CacheEntry *FindChild(CacheEntry &entry, Span<const char> name)
{
    RG_ASSERT(entry.directory.ready);

    for (CacheEntry &child: entry.directory.children) {
        if (TestStr(child.name, name))
            return &child;
    }

    return nullptr;
}

static void CopyAttributes(const rk_ObjectInfo &obj, CacheEntry *out_entry)
{
    switch (obj.type) {
        case rk_ObjectType::File: {
            out_entry->st.st_mode = S_IFREG | obj.mode;
            out_entry->st.st_size = (off_t)obj.size;
        } break;

        case rk_ObjectType::Directory:
        case rk_ObjectType::Snapshot: {
            out_entry->st.st_mode = S_IFDIR | obj.mode;
            out_entry->st.st_nlink = (nlink_t)2;
        } break;

        case rk_ObjectType::Link: {
            out_entry->st.st_mode = S_IFLNK | obj.mode;
        } break;

        case rk_ObjectType::Unknown: { RG_UNREACHABLE(); } break;
    }

    out_entry->st.st_uid = obj.uid;
    out_entry->st.st_gid = obj.gid;

#if defined(__linux__)
    out_entry->st.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->st.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->st.st_ctim.tv_sec = obj.btime / 1000;
    out_entry->st.st_ctim.tv_nsec = (obj.btime % 1000) * 1000000;
    out_entry->st.st_atim = out_entry->st.st_mtim;
#elif defined(__APPLE__)
    out_entry->st.st_mtimespec.tv_sec = obj.mtime / 1000;
    out_entry->st.st_mtimespec.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->st.st_birthtimespec.tv_sec = obj.btime / 1000;
    out_entry->st.st_birthtimespec.tv_nsec = (obj.btime % 1000) * 1000000;
    out_entry->st.st_atimespec = out_entry->st.st_mtimespec;
#elif defined(__OpenBSD__)
    out_entry->st.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->st.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->st.__st_birthtim.tv_sec = obj.btime / 1000;
    out_entry->st.__st_birthtim.tv_nsec = (obj.btime % 1000) * 1000000;
    out_entry->st.st_atim = out_entry->st.st_mtim;
#elif defined(__FreeBSD__)
    out_entry->st.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->st.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->st.st_birthtim.tv_sec = obj.btime / 1000;
    out_entry->st.st_birthtim.tv_nsec = (obj.btime % 1000) * 1000000;
    out_entry->st.st_atim = out_entry->st.st_mtim;
#else
    out_entry->st.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->st.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->st.st_ctim.tv_sec = obj.btime / 1000;
    out_entry->st.st_ctim.tv_nsec = (obj.btime % 1000) * 1000000;
    out_entry->st.st_atim = out_entry->st.st_mtim;
#endif
}

static bool InitRoot(const rk_Hash &hash)
{
    BlockAllocator temp_alloc;

    HeapArray<CacheEntry *> entries;

    root.parent = &root;
    root.name = "";
    root.st.st_mode = S_IFDIR | 0755;
    root.st.st_nlink = 2;
    root.directory.ready = true;
    root.refcount = 1;
    entries.Append(&root);

    HeapArray<rk_ObjectInfo> objects;
    if (!rk_List(disk.get(), hash, {}, &temp_alloc, &objects))
        return false;

    for (const rk_ObjectInfo &obj: objects) {
        if (obj.type == rk_ObjectType::Snapshot || obj.type == rk_ObjectType::Unknown)
            continue;

        CacheEntry *entry = &root;
        Span<const char> remain = obj.name;

        while (remain.len) {
            entry->directory.ready = true;

            Span<const char> part = SplitStr(remain, '/', &remain);
            CacheEntry *child = FindChild(*entry, part);

            if (!child) {
                child = entry->directory.children.AppendDefault();

                child->parent = entry;
                child->name = DuplicateString(part, &entry->directory.str_alloc).ptr;
                CopyAttributes(obj, child);
                child->st.st_nlink = 2;
                child->refcount = 1;
                entries.Append(child);

                entry->st.st_nlink++;
            }

            entry = child;
        }

        entry->hash = obj.hash;
    }

    // Fix up fake nodes
    for (CacheEntry *entry: entries) {
        if (entry->directory.ready) {
            entry->hash = {};
            entry->st.st_mode = S_IFDIR | 0755;
            entry->st.st_uid = getuid();
            entry->st.st_gid = getgid();
        }
    }

    return true;
}

static bool CacheDirectoryChildren(CacheEntry *entry)
{
    std::lock_guard lock(entry->directory.mutex);

    if (!entry->directory.ready) {
        HeapArray<rk_ObjectInfo> objects;
        if (!rk_List(disk.get(), entry->hash, {}, &entry->directory.str_alloc, &objects))
            return false;

        entry->directory.children.Reserve(objects.len);

        for (const rk_ObjectInfo &obj: objects) {
            if (obj.type == rk_ObjectType::Snapshot || obj.type == rk_ObjectType::Unknown) {
                LogWarning("Ignoring unexpected object in directory");
                continue;
            }

            CacheEntry *child = entry->directory.children.AppendDefault();

            child->parent = entry;
            child->name = obj.name;
            child->hash = obj.hash;

            CopyAttributes(obj, child);
        }

        entry->directory.ready = true;
    }

    return true;
}

static bool ResolveEntry(const char *path, CacheEntry **out_ptr)
{
    RG_ASSERT(path[0] == '/');

    Span<const char> remain = path + 1;
    CacheEntry *entry = &root;

    while (remain.len && entry) {
        if (!CacheDirectoryChildren(entry))
            return false;

        Span<const char> part = SplitStr(remain, '/', &remain);
        entry = FindChild(*entry, part);
    }

    *out_ptr = entry;
    return true;
}

static bool ResolveEntry(const char *path, const CacheEntry **out_ptr)
{
    // Work around const craziness
    return ResolveEntry(path, (CacheEntry **)out_ptr);
}

static void *DoInit(fuse_conn_info *, fuse_config *cfg)
{
    cfg->kernel_cache = 1;
    cfg->nullpath_ok = 1;

    return nullptr;
}

static int DoGetAttr(const char *path, struct stat *stbuf, fuse_file_info *)
{
    RG_ASSERT(path[0] == '/');

    const CacheEntry *entry;
    if (!ResolveEntry(path, &entry))
        return -EIO;
    if (!entry)
        return -ENOENT;
    RG_DEFER { entry->Unref(); };

    memcpy(stbuf, &entry->st, RG_SIZE(entry->st));
    return 0;
}

static int DoReadLink(const char *path, char *buf, size_t size)
{
    RG_ASSERT(size >= 1);

    CacheEntry *entry;
    if (!ResolveEntry(path, &entry))
        return -EIO;
    if (!entry)
        return -ENOENT;
    RG_DEFER_N(err_guard) { entry->Unref(); };

    if (!S_ISLNK(entry->st.st_mode))
        return -ENOENT;

    if (!entry->link.ready) {
        entry->link.target = rk_ReadLink(disk.get(), entry->hash, &entry->parent->directory.str_alloc);
        entry->link.ready = true;
    }
    if (!entry->link.target)
        return -EIO;

    strncpy(buf, entry->link.target, size);
    buf[size - 1] = 0;

    return 0;
}

static int DoOpenDir(const char *path, fuse_file_info *fi)
{
    CacheEntry *entry;
    if (!ResolveEntry(path, &entry))
        return -EIO;
    if (!entry)
        return -ENOENT;
    RG_DEFER_N(err_guard) { entry->Unref(); };

    if (!S_ISDIR(entry->st.st_mode))
        return -ENOTDIR;
    if (!CacheDirectoryChildren(entry))
        return -EIO;

    fi->fh = (uintptr_t)entry;
    err_guard.Disable();

    return 0;
}

static int DoReleaseDir(const char *, fuse_file_info *fi)
{
    const CacheEntry *entry = (const CacheEntry *)fi->fh;
    entry->Unref();

    return 0;
}

static int DoReadDir(const char *, void *buf, fuse_fill_dir_t filler,
                     off_t offset, fuse_file_info *fi, fuse_readdir_flags)
{
    const CacheEntry *entry = (const CacheEntry *)fi->fh;

    // XXX: (fuse_fill_dir_flags)FUSE_FILL_DIR_PLUS?

    offset++;

#define FILL(Name, StPtr) \
        do { \
            int ret = filler(buf, (Name), (StPtr), offset++, (fuse_fill_dir_flags)0); \
            if (ret) \
                return ret; \
        } while (false)

    if (offset == 1) {
        FILL(".", &entry->st);
    }
    if (offset == 2) {
        FILL("..", &entry->parent->st);
    }

    for (Size i = (Size)offset - 3; i < entry->directory.children.len; i++) {
        const CacheEntry &child = entry->directory.children[i];
        FILL(child.name, &child.st);
    }

#undef FILL

    return 0;
}

static int DoOpen(const char *path, fuse_file_info *fi)
{
    const CacheEntry *entry;
    if (!ResolveEntry(path, &entry))
        return -EIO;
    if (!entry)
        return -ENOENT;
    RG_DEFER { entry->Unref(); };

    if (!S_ISREG(entry->st.st_mode))
        return -EINVAL;
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    std::unique_ptr<rk_FileReader> reader = rk_OpenFile(disk.get(), entry->hash);
    if (!reader)
        return -EIO;

    fi->fh = (uintptr_t)reader.release();
    return 0;
}

static int DoRelease(const char *, fuse_file_info *fi)
{
    rk_FileReader *reader = (rk_FileReader *)fi->fh;
    delete reader;

    return 0;
}

static int DoRead(const char *, char *buf, size_t size, off_t offset, fuse_file_info *fi)
{
    rk_FileReader *reader = (rk_FileReader *)fi->fh;

    Span<uint8_t> dest = MakeSpan((uint8_t *)buf, (Size)size);
    Size read = reader->Read(offset, dest);

    if (read < 0)
        return -EIO;

    return (int)read;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const struct fuse_operations FuseOperations = {
    .getattr = DoGetAttr,
    .readlink = DoReadLink,
    .open = DoOpen,
    .read = DoRead,
    .release = DoRelease,
    .opendir = DoOpenDir,
    .readdir = DoReadDir,
    .releasedir = DoReleaseDir,
    .init = DoInit
};

#pragma GCC diagnostic pop

int RunMount(Span<const char *> arguments)
{
    // Options
    rk_Config config;
    const char *mountpoint = nullptr;
    rk_Hash hash = {};
    bool foreground = false;
    bool debug = false;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 mount [-R <repo>] <hash> <mountpoint>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password

    %!..+-j, --threads <threads>%!0      Change number of threads
                                 %!D..(default: automatic)%!0

    %!..+-f, --foreground%!0             Run mount process in foreground
        %!..+--debug%!0                  Debug FUSE calls)", FelixTarget);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                config.username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                config.password = opt.current_value;
            } else if (opt.Test("-j", "--threads", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &config.threads))
                    return 1;
                if (config.threads < 1) {
                    LogError("Threads count cannot be < 1");
                    return 1;
                }
            } else if (opt.Test("-f", "--foreground")) {
                foreground = true;
            } else if (opt.Test("--debug")) {
                debug = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *name = opt.ConsumeNonOption();
        mountpoint = opt.ConsumeNonOption();

        if (!name) {
            LogError("No hash provided");
            return 1;
        }
        if (!rk_ParseHash(name, &hash))
            return 1;
        if (!mountpoint) {
            LogError("Missing mountpoint");
            return 1;
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(true))
        return 1;

    disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::Full) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    if (!InitRoot(hash))
        return 1;

    // Run fuse main
    {
        HeapArray<const char *> argv;

        argv.Append("");
        if (foreground) {
            argv.Append("-f");
        }
        if (debug) {
            argv.Append("-d");
        }
        argv.Append(mountpoint);

        int ret = fuse_main((int)argv.len, (char **)argv.ptr, &FuseOperations, nullptr);
        return ret;
    }
}

}

#else

namespace RG {

int RunMount(Span<const char *>)
{
    LogError("Mount is not supported on this platform");
    return 1;
}

}

#endif
