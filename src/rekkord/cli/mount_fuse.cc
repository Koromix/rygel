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
#include "rekkord.hh"

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)

#if defined(__OpenBSD__)
    #include <fuse.h>
#else
    #include "vendor/libfuse/include/fuse.h"
#endif

namespace RG {

struct CacheEntry {
    CacheEntry *parent;

    const char *name;
    rk_ObjectID oid;
    struct stat sb;

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
static std::unique_ptr<rk_Repository> repo = nullptr;

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
            out_entry->sb.st_mode = S_IFREG | (obj.mode & ~S_IFMT);
            out_entry->sb.st_size = (off_t)obj.size;
            out_entry->sb.st_nlink = 1;
        } break;

        case rk_ObjectType::Directory:
        case rk_ObjectType::Snapshot: {
            out_entry->sb.st_mode = S_IFDIR | (obj.mode & ~S_IFMT);
            out_entry->sb.st_nlink = (nlink_t)(2 + obj.size);
        } break;

        case rk_ObjectType::Link: {
            out_entry->sb.st_mode = S_IFLNK | (obj.mode & ~S_IFMT);
            out_entry->sb.st_nlink = 1;
        } break;

        case rk_ObjectType::Unknown: { RG_UNREACHABLE(); } break;
    }

    out_entry->sb.st_uid = obj.uid;
    out_entry->sb.st_gid = obj.gid;

#if defined(__linux__)
    out_entry->sb.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->sb.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->sb.st_ctim.tv_sec = obj.ctime / 1000;
    out_entry->sb.st_ctim.tv_nsec = (obj.ctime % 1000) * 1000000;
    if (obj.atime) {
        out_entry->sb.st_atim.tv_sec = obj.atime / 1000;
        out_entry->sb.st_atim.tv_nsec = (obj.atime % 1000) * 1000000;
    } else {
        out_entry->sb.st_atim = out_entry->sb.st_mtim;
    }
#elif defined(__APPLE__)
    out_entry->sb.st_mtimespec.tv_sec = obj.mtime / 1000;
    out_entry->sb.st_mtimespec.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->sb.st_ctimespec.tv_sec = obj.ctime / 1000;
    out_entry->sb.st_ctimespec.tv_nsec = (obj.ctime % 1000) * 1000000;
    if (obj.atime) {
        out_entry->sb.st_atimespec.tv_sec = obj.atime / 1000;
        out_entry->sb.st_atimespec.tv_nsec = (obj.atime % 1000) * 1000000;
    } else {
        out_entry->sb.st_atimespec = out_entry->sb.st_mtimespec;
    }
    out_entry->sb.st_birthtimespec.tv_sec = obj.btime / 1000;
    out_entry->sb.st_birthtimespec.tv_nsec = (obj.btime % 1000) * 1000000;
#elif defined(__OpenBSD__)
    out_entry->sb.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->sb.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->sb.st_ctim.tv_sec = obj.ctime / 1000;
    out_entry->sb.st_ctim.tv_nsec = (obj.ctime % 1000) * 1000000;
    if (obj.atime) {
        out_entry->sb.st_atim.tv_sec = obj.atime / 1000;
        out_entry->sb.st_atim.tv_nsec = (obj.atime % 1000) * 1000000;
    } else {
        out_entry->sb.st_atim = out_entry->sb.st_mtim;
    }
    out_entry->sb.__st_birthtim.tv_sec = obj.btime / 1000;
    out_entry->sb.__st_birthtim.tv_nsec = (obj.btime % 1000) * 1000000;
#elif defined(__FreeBSD__)
    out_entry->sb.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->sb.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->sb.st_ctim.tv_sec = obj.ctime / 1000;
    out_entry->sb.st_ctim.tv_nsec = (obj.ctime % 1000) * 1000000;
    if (obj.atime) {
        out_entry->sb.st_atim.tv_sec = obj.atime / 1000;
        out_entry->sb.st_atim.tv_nsec = (obj.atime % 1000) * 1000000;
    } else {
        out_entry->sb.st_atim = out_entry->sb.st_mtim;
    }
    out_entry->sb.st_birthtim.tv_sec = obj.btime / 1000;
    out_entry->sb.st_birthtim.tv_nsec = (obj.btime % 1000) * 1000000;
#else
    out_entry->sb.st_mtim.tv_sec = obj.mtime / 1000;
    out_entry->sb.st_mtim.tv_nsec = (obj.mtime % 1000) * 1000000;
    out_entry->sb.st_ctim.tv_sec = obj.ctime / 1000;
    out_entry->sb.st_ctim.tv_nsec = (obj.ctime % 1000) * 1000000;
    if (obj.atime) {
        out_entry->sb.st_atim.tv_sec = obj.atime / 1000;
        out_entry->sb.st_atim.tv_nsec = (obj.atime % 1000) * 1000000;
    } else {
        out_entry->sb.st_atim = out_entry->sb.st_mtim;
    }
#endif
}

static bool InitRoot(const rk_ObjectID &oid)
{
    BlockAllocator temp_alloc;

    HeapArray<CacheEntry *> entries;

    root.parent = &root;
    root.name = "";
    root.sb.st_mode = S_IFDIR | 0755;
    root.sb.st_nlink = 2;
    root.directory.ready = true;
    root.refcount = 1;
    entries.Append(&root);

    HeapArray<rk_ObjectInfo> objects;
    if (!rk_ListChildren(repo.get(), oid, {}, &temp_alloc, &objects))
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
                child->sb.st_nlink = 2;
                child->refcount = 1;
                entries.Append(child);

                entry->sb.st_nlink++;
            }

            entry = child;
        }

        entry->oid = obj.oid;
        entry->sb.st_nlink = (nlink_t)(2 + obj.size);
    }

    // Fix up fake nodes
    for (CacheEntry *entry: entries) {
        if (entry->directory.ready) {
            entry->oid = {};
            entry->sb.st_mode = S_IFDIR | 0755;
            entry->sb.st_uid = getuid();
            entry->sb.st_gid = getgid();
        }
    }

    return true;
}

static bool CacheDirectoryChildren(CacheEntry *entry)
{
    RG_ASSERT(S_ISDIR(entry->sb.st_mode));

    std::lock_guard lock(entry->directory.mutex);

    if (!entry->directory.ready) {
        HeapArray<rk_ObjectInfo> objects;
        if (!rk_ListChildren(repo.get(), entry->oid, {}, &entry->directory.str_alloc, &objects))
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
            child->oid = obj.oid;

            CopyAttributes(obj, child);
        }

        entry->directory.ready = true;
    }

    return true;
}

static int ResolveEntry(const char *path, CacheEntry **out_ptr)
{
    RG_ASSERT(path[0] == '/');

    Span<const char> remain = path + 1;
    CacheEntry *entry = &root;

    while (remain.len) {
        if (!S_ISDIR(entry->sb.st_mode))
            return -ENOTDIR;
        if (!CacheDirectoryChildren(entry))
            return -EIO;

        Span<const char> part = SplitStr(remain, '/', &remain);
        entry = FindChild(*entry, part);

        if (!entry)
            return -ENOENT;
    }

    *out_ptr = entry;
    return 0;
}

static int ResolveEntry(const char *path, const CacheEntry **out_ptr)
{
    // Work around const craziness
    return ResolveEntry(path, (CacheEntry **)out_ptr);
}

#if !defined(__OpenBSD__)
static void *DoInit(fuse_conn_info *conn, fuse_config *cfg)
{
    cfg->kernel_cache = 1;
    cfg->nullpath_ok = 1;

    cfg->entry_timeout = 3600.0;
    cfg->attr_timeout = 3600.0;
    cfg->negative_timeout = 3600.0;

    conn->max_read = Mebibytes(4);
    conn->max_background = 4 * GetCoreCount();
    conn->max_readahead = Mebibytes(4);

    return nullptr;
}
#endif

#if defined(__OpenBSD__)
static int DoGetAttr(const char *path, struct stat *stbuf)
#else
static int DoGetAttr(const char *path, struct stat *stbuf, fuse_file_info *)
#endif
{
    const CacheEntry *entry;
    if (int error = ResolveEntry(path, &entry); error < 0) {
        MemSet(stbuf, 0, RG_SIZE(*stbuf));
        return error;
    }
    RG_DEFER { entry->Unref(); };

    MemCpy(stbuf, &entry->sb, RG_SIZE(entry->sb));
    return 0;
}

static int DoReadLink(const char *path, char *buf, size_t size)
{
    RG_ASSERT(size >= 1);

    CacheEntry *entry;
    if (int error = ResolveEntry(path, &entry); error < 0)
        return error;
    RG_DEFER_N(err_guard) { entry->Unref(); };

    if (!S_ISLNK(entry->sb.st_mode))
        return -ENOENT;

    if (!entry->link.ready) {
        entry->link.target = rk_ReadLink(repo.get(), entry->oid, &entry->parent->directory.str_alloc);
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
    if (int error = ResolveEntry(path, &entry); error < 0)
        return error;
    RG_DEFER_N(err_guard) { entry->Unref(); };

    if (!S_ISDIR(entry->sb.st_mode))
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

#if defined(__OpenBSD__)
static int DoReadDir(const char *, void *buf, fuse_fill_dir_t filler,
                     off_t, fuse_file_info *fi)
#else
static int DoReadDir(const char *, void *buf, fuse_fill_dir_t filler,
                     off_t, fuse_file_info *fi, fuse_readdir_flags)
#endif
{
    const CacheEntry *entry = (const CacheEntry *)fi->fh;

#if defined(__OpenBSD__)
    #define FILL(Name, StPtr) \
            filler(buf, (Name), (StPtr), 0)
#else
    #define FILL(Name, StPtr) \
            filler(buf, (Name), (StPtr), 0, (fuse_fill_dir_flags)FUSE_FILL_DIR_PLUS)
#endif

    FILL(".", &entry->sb);
    FILL("..", &entry->parent->sb);

    for (const CacheEntry &child: entry->directory.children) {
        FILL(child.name, &child.sb);
    }

#undef FILL

    return 0;
}

static int DoOpen(const char *path, fuse_file_info *fi)
{
    const CacheEntry *entry;
    if (int error = ResolveEntry(path, &entry); error < 0)
        return error;
    RG_DEFER { entry->Unref(); };

    if (!S_ISREG(entry->sb.st_mode))
        return -EINVAL;
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    std::unique_ptr<rk_FileHandle> handle = rk_OpenFile(repo.get(), entry->oid);
    if (!handle)
        return -EIO;

    fi->fh = (uintptr_t)handle.release();
    return 0;
}

static int DoRelease(const char *, fuse_file_info *fi)
{
    rk_FileHandle *handle = (rk_FileHandle *)fi->fh;
    delete handle;

    return 0;
}

static int DoRead(const char *, char *buf, size_t size, off_t offset, fuse_file_info *fi)
{
    rk_FileHandle *handle = (rk_FileHandle *)fi->fh;

    Span<uint8_t> dest = MakeSpan((uint8_t *)buf, (Size)size);
    Size read = handle->Read(offset, dest);

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
#if !defined(__OpenBSD__)
    .init = DoInit
#endif
};

#pragma GCC diagnostic pop

int RunMount(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    bool foreground = false;
    bool debug = false;
    HeapArray<const char *> fuse_options;
    const char *identifier = nullptr;
    const char *mountpoint = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 mount [-C filename] [option...] identifier mountpoint%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
Mount options:

    %!..+-f, --foreground%!0               Run mount process in foreground

        %!..+--auto_unmount%!0             Release filesystem automatically after process termination
        %!..+--allow_other%!0              Allow all users to access the filesystem
        %!..+--allow_root%!0               Allow owner and root to access the filesystem
        %!..+--default_permissions%!0      Enforce snapshotted file permissions

        %!..+--debug%!0                    Debug FUSE calls

Use an object ID (OID) or a snapshot channel as the identifier. You can append an optional path (separated by a colon), the full syntax for object identifiers is %!..+<OID|channel>[:<path>]%!0.
If you use a snapshot channel, the most recent snapshot object that matches will be used.)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-f", "--foreground")) {
                foreground = true;
            } else if (opt.Test("--debug")) {
                debug = true;
            } else if (opt.Test("--auto_unmount")) {
                fuse_options.Append("auto_unmount");
            } else if (opt.Test("--default_permissions")) {
                fuse_options.Append("default_permissions");
            } else if (opt.Test("--allow_other")) {
                fuse_options.Append("allow_other");
            } else if (opt.Test("--allow_root")) {
                fuse_options.Append("allow_root");
            } else if (opt.Test("--owner_root")) {
                fuse_options.Append("owner_root");
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        identifier = opt.ConsumeNonOption();
        mountpoint = opt.ConsumeNonOption();

        opt.LogUnusedArguments();
    }

    if (!identifier) {
        LogError("No identifier provided");
        return 1;
    }
    if (!mountpoint) {
        LogError("Missing mountpoint");
        return 1;
    }

    // Normalize mount point
    mountpoint = Fmt(&temp_alloc, "%1/", TrimStrRight(mountpoint, '/')).ptr;

    // Check mount point ahead of time
    {
        FileInfo file_info;
        if (StatFile(mountpoint, &file_info) != StatResult::Success)
            return 1;
        if (file_info.type != FileType::Directory) {
            LogError("Mountpoint '%1' is not a directory", mountpoint);
            return 1;
        }
    }

    if (!rekkord_config.Complete(true))
        return 1;

    // We keep these objects in globals for simplicity, but we need to destroy them at the end
    // of this function, before some exit functions (such as ssh_finalize) are called.
    disk = rk_OpenDisk(rekkord_config);
    repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;
    RG_DEFER {
        repo.reset(nullptr);
        disk.reset(nullptr);
    };

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Read)) {
        LogError("Cannot mount with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    rk_ObjectID oid = {};
    if (!rk_LocateObject(repo.get(), identifier, &oid))
        return 1;

    LogInfo("Mounting %!..+%1%!0 to '%2'...", oid, mountpoint);
    if (!InitRoot(oid))
        return 1;
    LogInfo("Ready");

#if !defined(__OpenBSD__)
    // This option needs to be specified twice for some reason
    const char *max_read = Fmt(&temp_alloc, "max_read=%1", Mebibytes(4)).ptr;
    fuse_options.Append(max_read);
#endif

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
        for (const char *opt: fuse_options) {
            argv.Append("-o");
            argv.Append(opt);
        }
        argv.Append(mountpoint);

        int ret = fuse_main((int)argv.len, (char **)argv.ptr, &FuseOperations, nullptr);
        return ret;
    }
}

}

#endif
