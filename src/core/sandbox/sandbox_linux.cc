// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#if defined(__linux__)

#include "src/core/base/base.hh"
#include "sandbox.hh"

#include <fcntl.h>
#include <sched.h>
#include "vendor/libseccomp/include/seccomp.h"
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>

// For some reason sys/capability.h is in some crap separate package, because why make
// it simple when you could instead make it a mess and require users to install
// random distribution-specifc packages to do anything of value.
#define _LINUX_CAPABILITY_VERSION_3 0x20080522
struct cap_user_header {
    uint32_t version;
    int pid;
};
struct cap_user_data {
    uint32_t effective;
    uint32_t permitted;
    uint32_t inheritable;
};

// Distribution is probably shipping an old linux/landlock.h header version.
#if !defined(__NR_landlock_create_ruleset)
    #define __NR_landlock_create_ruleset 444
#endif
#if !defined(__NR_landlock_add_rule)
    #define __NR_landlock_add_rule 445
#endif
#if !defined(__NR_landlock_restrict_self)
    #define __NR_landlock_restrict_self 446
#endif
#define LANDLOCK_WARN_ABI 6
#define LANDLOCK_CREATE_RULESET_VERSION (1u << 0)
enum landlock_rule_type {
    LANDLOCK_RULE_PATH_BENEATH = 1,
    LANDLOCK_RULE_NET_PORT,
};
struct landlock_ruleset_attr {
    uint64_t handled_access_fs;
    uint64_t handled_access_net;
    uint64_t scoped;
} __attribute__((packed));
struct landlock_path_beneath_attr {
    uint64_t allowed_access;
    int32_t parent_fd;
} __attribute__((packed));
struct landlock_net_port_attr {
    uint64_t allowed_access;
    uint64_t port;
} __attribute__((packed));
#define LANDLOCK_ACCESS_FS_EXECUTE          (1ull << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE       (1ull << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE        (1ull << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR         (1ull << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR       (1ull << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE      (1ull << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR        (1ull << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR         (1ull << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG         (1ull << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK        (1ull << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO        (1ull << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK       (1ull << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM         (1ull << 12)
#define LANDLOCK_ACCESS_FS_REFER            (1ull << 13)
#define LANDLOCK_ACCESS_FS_TRUNCATE         (1ull << 14)
#define LANDLOCK_ACCESS_FS_IOCTL_DEV        (1ull << 15)
#define LANDLOCK_ACCESS_NET_BIND_TCP        (1ull << 0)
#define LANDLOCK_ACCESS_NET_CONNECT_TCP     (1ull << 1)
#define LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET (1ull << 0)
#define LANDLOCK_SCOPE_SIGNAL               (1ull << 1)

#define ACCESS_FS_READ (LANDLOCK_ACCESS_FS_EXECUTE | \
                        LANDLOCK_ACCESS_FS_READ_FILE | \
                        LANDLOCK_ACCESS_FS_READ_DIR)
#define ACCESS_FS_WRITE (LANDLOCK_ACCESS_FS_WRITE_FILE | \
                         LANDLOCK_ACCESS_FS_REMOVE_DIR | \
                         LANDLOCK_ACCESS_FS_REMOVE_FILE | \
                         LANDLOCK_ACCESS_FS_MAKE_CHAR | \
                         LANDLOCK_ACCESS_FS_MAKE_DIR | \
                         LANDLOCK_ACCESS_FS_MAKE_REG | \
                         LANDLOCK_ACCESS_FS_MAKE_SOCK | \
                         LANDLOCK_ACCESS_FS_MAKE_FIFO | \
                         LANDLOCK_ACCESS_FS_MAKE_BLOCK | \
                         LANDLOCK_ACCESS_FS_MAKE_SYM | \
                         LANDLOCK_ACCESS_FS_REFER | \
                         LANDLOCK_ACCESS_FS_TRUNCATE | \
                         LANDLOCK_ACCESS_FS_IOCTL_DEV)
#define ACCESS_FILE (LANDLOCK_ACCESS_FS_EXECUTE | \
                     LANDLOCK_ACCESS_FS_WRITE_FILE | \
                     LANDLOCK_ACCESS_FS_READ_FILE | \
                     LANDLOCK_ACCESS_FS_TRUNCATE | \
                     LANDLOCK_ACCESS_FS_IOCTL_DEV)

namespace K {

bool sb_SandboxBuilder::Init(unsigned int flags)
{
    K_ASSERT(!isolation);
    K_ASSERT(flags);

#if defined(__SANITIZE_ADDRESS__)
    LogError("Sandboxing does not support AddressSanitizer");
    return false;
#elif defined(__SANITIZE_THREAD__)
    LogError("Sandboxing does not support ThreadSanitizer");
    return false;
#endif

    isolation = flags;
    return true;
}

void sb_SandboxBuilder::RevealPaths(Span<const sb_RevealedPath> paths)
{
    for (sb_RevealedPath reveal: paths) {
        K_ASSERT(reveal.path[0] == '/');

        reveal.path = DuplicateString(TrimStrRight(reveal.path, K_PATH_SEPARATORS), &str_alloc).ptr;
        reveals.Append(reveal);
    }
}

void sb_SandboxBuilder::RevealPaths(Span<const char *const> paths, bool readonly)
{
    for (const char *path: paths) {
        K_ASSERT(path[0] == '/');

        sb_RevealedPath reveal = {};

        reveal.path = DuplicateString(TrimStrRight(path, K_PATH_SEPARATORS), &str_alloc).ptr;
        reveal.readonly = readonly;

        reveals.Append(reveal);
    }
}

void sb_SandboxBuilder::FilterSyscalls(Span<const sb_SyscallFilter> filters)
{
    for (sb_SyscallFilter filter: filters) {
        filter.name = DuplicateString(filter.name, &str_alloc).ptr;
        this->filters.Append(filter);
    }
}

static bool DropCapabilities()
{
    static bool dropped = false;

    if (!dropped) {
        LogDebug("Dropping all capabilities");

        for (int i = 0; i < 64; i++) {
            if (prctl(PR_CAPBSET_DROP, i, 0, 0, 0) < 0 && errno != EINVAL && errno != EPERM) {
                LogError("Failed to drop bounding capability set: %1", strerror(errno));
                return false;
            }
        }
        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) < 0 && errno != EINVAL) {
            LogError("Failed to clear ambient capability set: %1", strerror(errno));
            return false;
        }

        cap_user_header hdr = { _LINUX_CAPABILITY_VERSION_3, 0 };
        cap_user_data data[2] = {};

        if (syscall(__NR_capset, &hdr, data) < 0) {
            LogError("Failed to drop capabilities: %1", strerror(errno));
            return false;
        }

        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
            LogError("Failed to restrict privileges: %1", strerror(errno));
            return false;
        }
        if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) < 0) {
            LogError("Failed to clear dumpable proc attribute: %1", strerror(errno));
            return false;
        }

        dropped = true;
    }

    return true;
}

static bool InitLandlock(unsigned int flags, Span<const sb_RevealedPath> reveals)
{
    LogDebug("Using Landlock for process isolation");

    landlock_ruleset_attr attr = {};
    {
        attr.handled_access_fs = ACCESS_FS_READ | ACCESS_FS_WRITE;
        attr.handled_access_net = LANDLOCK_ACCESS_NET_BIND_TCP | LANDLOCK_ACCESS_NET_CONNECT_TCP;
        attr.scoped = LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET;

        if (flags & (int)sb_IsolationFlag::Signals) {
            attr.scoped |= LANDLOCK_SCOPE_SIGNAL;
        }

        int abi = syscall(__NR_landlock_create_ruleset, nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION);
        if (abi < 0) {
            LogError("Failed to use Landlock for sandboxing: %1", strerror(errno));
            return false;
        }

        switch (abi) {
            case 1: {
                attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_REFER;
            } [[fallthrough]];
            case 2: {
                attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_TRUNCATE;
            } [[fallthrough]];
            case 3: {
                attr.handled_access_net &= ~LANDLOCK_ACCESS_NET_BIND_TCP;
                attr.handled_access_net &= ~LANDLOCK_ACCESS_NET_CONNECT_TCP;
            } [[fallthrough]];
            case 4: {
                attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_IOCTL_DEV;
            } [[fallthrough]];
            case 5: {
                attr.scoped &= ~LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET;
                attr.scoped &= ~LANDLOCK_SCOPE_SIGNAL;
            } // [[fallthrough]];
        }

        if (abi < LANDLOCK_WARN_ABI) {
            LogWarning("Update the running kernel to leverage Landlock features provided by ABI version %1", LANDLOCK_WARN_ABI);
        }
    }

    if (!DropCapabilities())
        return false;

    int fd = syscall(__NR_landlock_create_ruleset, &attr, K_SIZE(attr), 0);
    if (fd < 0) {
        LogError("Failed to create Landlock ruleset: %1", strerror(errno));
        return false;
    }
    K_DEFER { CloseDescriptor(fd); };

    for (const sb_RevealedPath &reveal: reveals) {
        struct landlock_path_beneath_attr beneath = {};

        beneath.parent_fd = open(reveal.path, O_PATH | O_CLOEXEC);
        if (beneath.parent_fd < 0) {
            LogError("Failed to open '%1': %2", reveal.path, strerror(errno));
            return false;
        }
        K_DEFER { close(beneath.parent_fd); };

        struct stat sb;
        if (fstat(beneath.parent_fd, &sb) < 0) {
            LogError("Failed to stat '%1': %2", reveal.path, strerror(errno));
            return false;
        }

        beneath.allowed_access = ACCESS_FS_READ;
        if (!reveal.readonly) {
            beneath.allowed_access |= ACCESS_FS_WRITE;
        }
        if (!S_ISDIR(sb.st_mode)) {
            beneath.allowed_access &= ACCESS_FILE;
        }
        beneath.allowed_access &= attr.handled_access_fs;

        if (syscall(__NR_landlock_add_rule, fd, LANDLOCK_RULE_PATH_BENEATH, &beneath, 0) < 0) {
            LogError("Failed to add Landlock rule for '%1': %2", reveal.path, strerror(errno));
            return false;
        }
    }

    if (syscall(__NR_landlock_restrict_self, fd, 0) < 0) {
        LogError("Failed to apply Landlock restrictions: %1", strerror(errno));
        return false;
    }

    return true;
}

static bool WriteUidGidMap(pid_t pid, uid_t uid, gid_t gid)
{
    int uid_fd;
    {
        char buf[512];
        Fmt(buf, "/proc/%1/uid_map", pid);

        uid_fd = K_RESTART_EINTR(open(buf, O_CLOEXEC | O_WRONLY), < 0);
        if (uid_fd < 0) {
            LogError("Failed to open '%1' for writing: %2", buf, strerror(errno));
            return false;
        }
    }
    K_DEFER { close(uid_fd); };

    int gid_fd;
    {
        char buf[512];
        Fmt(buf, "/proc/%1/gid_map", pid);

        gid_fd = K_RESTART_EINTR(open(buf, O_CLOEXEC | O_WRONLY), < 0);
        if (gid_fd < 0) {
            LogError("Failed to open '%1' for writing: %2", buf, strerror(errno));
            return false;
        }
    }
    K_DEFER { close(gid_fd); };

    // More random crap Linux wants us to do, or writing GID map fails in unprivileged mode
    {
        char buf[512];
        Fmt(buf, "/proc/%1/setgroups", pid);

        if (!WriteFile("deny", buf))
            return false;
    }

    // Write UID map
    {
        LocalArray<char, 512> buf;

        buf.len = Fmt(buf.data, "%1 %1 1\n", uid).len;
        if (K_RESTART_EINTR(write(uid_fd, buf.data, buf.len), < 0) < 0) {
            LogError("Failed to write UID map: %1", strerror(errno));
            return false;
        }
    }

    // Write GID map
    {
        LocalArray<char, 512> buf;

        buf.len = Fmt(buf.data, "%1 %1 1\n", gid).len;
        if (K_RESTART_EINTR(write(gid_fd, buf.data, buf.len), < 0) < 0) {
            LogError("Failed to write GID map: %1", strerror(errno));
            return false;
        }
    }

    return true;
}

static bool DoUnshare()
{
    uint32_t flags = CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWIPC |
                     CLONE_NEWUTS | CLONE_NEWCGROUP | CLONE_THREAD;

    if (unshare(flags) < 0) {
        LogError("Failed to create namespace: %1", strerror(errno));
        return false;
    }

    return true;
}

static bool InitNamespaces(unsigned int, Span<const sb_RevealedPath> reveals)
{
    LogDebug("Using Linux namespaces for process isolation");

    BlockAllocator temp_alloc;

    uid_t uid = getuid();
    gid_t gid = getgid();

    if (!uid) {
        int random_id = GetRandomInt(58000, 60000);

        static_assert(K_SIZE(uid_t) >= K_SIZE(int));
        static_assert(K_SIZE(gid_t) >= K_SIZE(int));

        uid = random_id;
        gid = random_id;
    }

    // We support two namespacing code paths: unprivileged, or CAP_SYS_ADMIN (root).
    // First, decide between the two. Unprivileged is simpler but it requires a
    // relatively recent kernel, and may be disabled (Debian). If we have CAP_SYS_ADMIN,
    // or if we can acquire it, use it instead.
    bool privileged = !geteuid();
    if (!privileged) {
        cap_user_header hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
        cap_user_data data[2];

        if (syscall(__NR_capget, &hdr, data) < 0) {
            LogError("Failed to read process capabilities: %1", strerror(errno));
            return false;
        }

        if (data[0].effective & (1u << 21)) { // CAP_SYS_ADMIN
            privileged = true;
        } else if (data[0].permitted & (1u << 21)) {
            data[0].effective |= 1u << 21;

            if (syscall(__NR_capset, &hdr, data) >= 0) {
                privileged = true;
            } else {
                LogDebug("Failed to enable CAP_SYS_ADMIN (despite it being permitted): %1", strerror(errno));
            }
        }
    }

    // Setup user namespace
    if (privileged) {
        // In the privileged path, we need to fork a child process, which keeps root privileges
        // and writes the UID and GID map of the namespaced parent process, because I can't find
        // any way to do it simply otherwise (EPERM). The child process exits immediately
        // once this is done.
        LogDebug("Trying CAP_SYS_ADMIN (root) sandbox method");

        // We use this dummy event to wait in the child process until the parent
        // process has called unshare() successfully.
        int efd = eventfd(0, EFD_CLOEXEC);
        if (efd < 0) {
            LogError("Failed to create eventfd: %1", strerror(errno));
            return false;
        }
        K_DEFER { close(efd); };

        pid_t child_pid = fork();
        if (child_pid < 0) {
            LogError("Failed to fork: %1", strerror(errno));
            return false;
        }

        if (child_pid) {
            K_DEFER_N(kill_guard) {
                kill(child_pid, SIGKILL);
                waitpid(child_pid, nullptr, 0);
            };

            // This allows the sandbox helper to write to our /proc files even when
            // running as non-root in the CAP_SYS_ADMIN sandbox path.
            prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);

            int64_t dummy = 1;
            if (!DoUnshare())
                return false;
            if (K_RESTART_EINTR(write(efd, &dummy, K_SIZE(dummy)), < 0) < 0) {
                LogError("Failed to write to eventfd: %1", strerror(errno));
                return false;
            }

            // Good to go! After a successful write to eventfd, the child WILL exit
            // so we can just wait for that.
            kill_guard.Disable();

            int wstatus;
            if (waitpid(child_pid, &wstatus, 0) < 0) {
                LogError("Failed to wait for sandbox helper: %1", strerror(errno));
                return false;
            }
            if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
                LogDebug("Something went wrong in the sandbox helper");
                return false;
            }

            LogDebug("Change UID/GID to %1/%2", uid, gid);

            // Set non-root container UID and GID
            if (setresuid(uid, uid, uid) < 0 || setresgid(gid, gid, gid) < 0) {
                LogError("Cannot change UID or GID: %1", strerror(errno));
                return false;
            }
        } else {
            int64_t dummy;
            if (K_RESTART_EINTR(read(efd, &dummy, K_SIZE(dummy)), < 0) < 0) {
                LogError("Failed to read eventfd: %1", strerror(errno));
                _exit(1);
            }

            bool success = WriteUidGidMap(getppid(), uid, gid);
            _exit(!success);
        }
    } else {
        LogDebug("Trying unprivileged sandbox method");

        if (!DoUnshare())
            return false;
        if (!WriteUidGidMap(getpid(), uid, gid))
            return false;
    }

    // Set up FS namespace
    {
        if (!MakeDirectory("/tmp/sandbox", false))
            return false;
        if (mount("tmpfs", "/tmp/sandbox", "tmpfs", 0, "size=4k") < 0 && errno != EBUSY) {
            LogError("Failed to mount tmpfs on '/tmp/sandbox': %1", strerror(errno));
            return false;
        }
        if (mount(nullptr, "/tmp/sandbox", nullptr, MS_PRIVATE, nullptr) < 0) {
            LogError("Failed to set MS_PRIVATE on '/tmp/sandbox': %1", strerror(errno));
            return false;
        }

        // Create root FS with tmpfs
        const char *fs_root = CreateUniqueDirectory("/tmp/sandbox", nullptr, &temp_alloc);
        if (!fs_root)
            return false;
        if (mount("tmpfs", fs_root, "tmpfs", 0, "size=1M,mode=0700") < 0) {
            LogError("Failed to mount tmpfs on '%1': %2", fs_root, strerror(errno));
            return false;
        }
        if (mount(nullptr, fs_root, nullptr, MS_PRIVATE, nullptr) < 0) {
            LogError("Failed to set MS_PRIVATE on '%1': %2", fs_root, strerror(errno));
            return false;
        }
        LogDebug("Sandbox FS root: '%1'", fs_root);

        // Mount requested paths
        for (const sb_RevealedPath &reveal: reveals) {
            const char *dest = Fmt(&temp_alloc, "%1%2", fs_root, reveal.path).ptr;
            int flags = MS_BIND | MS_REC | (reveal.readonly ? MS_RDONLY : 0);

            // Deal with special cases
            if (TestStr(reveal.path, "/proc/self")) {
                char path[256];
                Fmt(path, "/proc/%1", getpid());

                if (!MakeDirectoryRec(dest))
                    return false;

                if (mount(path, dest, nullptr, flags, nullptr) < 0) {
                    LogError("Failed to mount '/proc/self': %1", strerror(errno));
                    return false;
                }

                continue;
            }

            // Ensure destination exists
            {
                FileInfo src_info;
                if (StatFile(reveal.path, &src_info) != StatResult::Success)
                    return false;

                if (src_info.type == FileType::Directory) {
                    if (!MakeDirectoryRec(dest))
                        return false;
                } else {
                    if (!EnsureDirectoryExists(dest))
                        return false;

                    int fd = OpenFile(dest, (int)OpenFlag::Write);
                    if (fd < 0)
                        return false;
                    close(fd);
                }
            }

            if (mount(reveal.path, dest, nullptr, flags, nullptr) < 0) {
                LogError("Failed to mount '%1' to '%2': %3", reveal.path, dest, strerror(errno));
                return false;
            }
        }

        // Pivot! Pivot! Pivot!
        {
            int old_root_fd = K_RESTART_EINTR(open("/", O_DIRECTORY | O_PATH), < 0);
            if (old_root_fd < 0) {
                LogError("Failed to open directory '/': %1", strerror(errno));
                return false;
            }
            K_DEFER { close(old_root_fd); };

            int new_root_fd = K_RESTART_EINTR(open(fs_root, O_DIRECTORY | O_PATH), < 0);
            if (new_root_fd < 0) {
                LogError("Failed to open directory '%1': %2", fs_root, strerror(errno));
                return false;
            }
            K_DEFER { close(new_root_fd); };

            if (fchdir(new_root_fd) < 0) {
                LogError("Failed to change current directory to '%1': %2", fs_root, strerror(errno));
                return false;
            }
            if (syscall(__NR_pivot_root, ".", ".") < 0) {
                LogError("Failed to pivot root mount point: %1", strerror(errno));
                return false;
            }
            if (fchdir(old_root_fd) < 0) {
                LogError("Failed to change current directory to old '/': %1", strerror(errno));
                return false;
            }

            if (mount(nullptr, ".", nullptr, MS_REC | MS_PRIVATE, nullptr) < 0) {
                LogError("Failed to set MS_PRIVATE on '%1': %2", fs_root, strerror(errno));
                return false;
            }

            // I don't know why there's a loop below but I've seen it done.
            // But at least this is true to the real Unix and Linux philosophy: silly nonsensical API
            // and complete lack of taste and foresight.
            if (umount2(".", MNT_DETACH) < 0) {
                LogError("Failed to unmount old root mount point: %1", strerror(errno));
                return false;
            }
            for (;;) {
                if (umount2(".", MNT_DETACH) < 0) {
                    if (errno == EINVAL) {
                        break;
                    } else {
                        LogError("Failed to unmount old root mount point: %1", strerror(errno));
                        return false;
                    }
                }
            }
        }

        // Set current working directory
        if (chdir("/") < 0) {
            LogError("Failed to change current directory to new '/': %1", strerror(errno));
            return false;
        }
    }

    if (!DropCapabilities())
        return false;

    return true;
}

static bool InitSeccomp(Span<const sb_SyscallFilter> filters)
{
    LogDebug("Applying syscall filters");

    sb_FilterAction default_action;
    {
        const char *str = GetEnv("DEFAULT_SECCOMP_ACTION");

        if (!str || TestStrI(str, "Kill")) {
            default_action = sb_FilterAction::Kill;
        } else if (TestStrI(str, "Log")) {
            default_action = sb_FilterAction::Log;
        } else if (TestStrI(str, "Block")) {
            default_action = sb_FilterAction::Block;
        } else if (TestStrI(str, "Trap")) {
            default_action = sb_FilterAction::Trap;
        } else {
            LogError("Invalid default seccomp action '%1'", str);
            return false;
        }
    }

    if (prctl(PR_GET_SECCOMP, 0, 0, 0, 0) < 0) {
        LogError("Cannot sandbox syscalls: seccomp is not available");
        return false;
    }

    // Check support for KILL_PROCESS action
    uint32_t kill_code = SCMP_ACT_KILL_PROCESS;
    if (syscall(__NR_seccomp, 2, 0, &kill_code) < 0) { // SECCOMP_GET_ACTION_AVAIL
        LogDebug("Seccomp action KILL_PROCESS is not available; falling back to KILL_THREAD");
        kill_code = SCMP_ACT_KILL_THREAD;
    }

    const auto translate_action = [&](sb_FilterAction action) {
        uint32_t seccomp_action = UINT32_MAX;
        switch (action) {
            case sb_FilterAction::Allow: { seccomp_action = SCMP_ACT_ALLOW; } break;
            case sb_FilterAction::Log: { seccomp_action = SCMP_ACT_LOG; } break;
            case sb_FilterAction::Block: { seccomp_action = SCMP_ACT_ERRNO(EPERM); } break;
            case sb_FilterAction::Trap: { seccomp_action = SCMP_ACT_TRAP; } break;
            case sb_FilterAction::Kill: { seccomp_action = kill_code; } break;
        }
        K_ASSERT(seccomp_action != UINT32_MAX);

        return seccomp_action;
    };

    scmp_filter_ctx ctx = seccomp_init(translate_action(default_action));
    if (!ctx) {
        LogError("Cannot sandbox syscalls: seccomp_init() failed");
        return false;
    }
    K_DEFER { seccomp_release(ctx); };

    for (const sb_SyscallFilter &filter: filters) {
        if (filter.action != default_action) {
            int ret = 0;

            if (TestStr(filter.name, "ioctl/tty")) {
                int syscall = seccomp_syscall_resolve_name("ioctl");
                K_ASSERT(syscall != __NR_SCMP_ERROR);

                ret = seccomp_rule_add(ctx, translate_action(filter.action), syscall, 1,
                                       SCMP_A1(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFFFFFFF00ul, 0x5400u));
            } else if (TestStr(filter.name, "mmap/anon")) {
                int syscall = seccomp_syscall_resolve_name("mmap");
                K_ASSERT(syscall != __NR_SCMP_ERROR);

                unsigned int prot_mask = PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC;
                unsigned int prot_combinations[] = {
                    PROT_NONE,
                    PROT_READ,
                    PROT_WRITE,
                    PROT_READ | PROT_WRITE
                };
                unsigned int map_combinations[] = {
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE
                };

                for (unsigned int prot_flags: prot_combinations) {
                    for (unsigned int map_flags: map_combinations) {
                        ret = seccomp_rule_add(ctx, translate_action(filter.action), syscall, 3,
                                               SCMP_A2(SCMP_CMP_MASKED_EQ, prot_mask, prot_flags),
                                               SCMP_A3(SCMP_CMP_EQ, map_flags, 0),
                                               SCMP_A4(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, 0xFFFFFFFFu));
                        if (ret < 0)
                            break;
                    }
                }
            } else if (TestStr(filter.name, "mmap/shared")) {
                int syscall = seccomp_syscall_resolve_name("mmap");
                K_ASSERT(syscall != __NR_SCMP_ERROR);

                unsigned int mask = PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC;
                unsigned int combinations[] = {
                    PROT_NONE,
                    PROT_READ,
                    PROT_WRITE,
                    PROT_READ | PROT_WRITE
                };

                for (unsigned int flags: combinations) {
                    ret = seccomp_rule_add(ctx, translate_action(filter.action), syscall, 2,
                                           SCMP_A2(SCMP_CMP_MASKED_EQ, mask, flags),
                                           SCMP_A3(SCMP_CMP_EQ, MAP_SHARED, 0));
                    if (ret < 0)
                        break;
                }
            } else if (TestStr(filter.name, "mprotect/noexec")) {
                int syscall = seccomp_syscall_resolve_name("mprotect");
                K_ASSERT(syscall != __NR_SCMP_ERROR);

                unsigned int mask = PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC;
                unsigned int combinations[] = {
                    PROT_NONE,
                    PROT_READ,
                    PROT_WRITE,
                    PROT_READ | PROT_WRITE
                };

                for (unsigned int flags: combinations) {
                    ret = seccomp_rule_add(ctx, translate_action(filter.action), syscall, 1,
                                           SCMP_A2(SCMP_CMP_MASKED_EQ, mask, flags));
                    if (ret < 0)
                        break;
                }
            } else if (TestStr(filter.name, "clone/fork")) {
                int syscall = seccomp_syscall_resolve_name("clone");
                K_ASSERT(syscall != __NR_SCMP_ERROR);

                unsigned int mask = CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD;
                unsigned int combinations[] = {
                    SIGCHLD,
                    CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD
                };

                for (unsigned int flags: combinations) {
                    ret = seccomp_rule_add(ctx, translate_action(filter.action), syscall, 1,
                                           SCMP_A1(SCMP_CMP_MASKED_EQ, mask, flags));
                    if (ret < 0)
                        break;
                }
            } else {
                int syscall = seccomp_syscall_resolve_name(filter.name);

                if (syscall != __NR_SCMP_ERROR) {
                    ret = seccomp_rule_add(ctx, translate_action(filter.action), syscall, 0);
                } else {
                    if (strchr(filter.name, '/')) {
                        LogError("Unknown syscall specifier '%1'", filter.name);
                        return false;
                    } else {
                        LogDebug("Ignoring unknown syscall '%1'", filter.name);
                    }
                }
            }

            if (ret < 0) {
                LogError("Invalid seccomp syscall '%1': %2", filter.name, strerror(-ret));
                return false;
            }
        }
    }

    if (int ret = seccomp_load(ctx); ret < 0) {
        LogError("Failed to install syscall filters: %1", strerror(-ret));
        return false;
    }

    return true;
}

bool sb_SandboxBuilder::Apply()
{
    K_ASSERT(isolation);

    if (isolation & (int)sb_IsolationFlag::Filesystem) {
        const char *str = GetEnv("SANDBOX_METHOD");

        if (str) {
            if (TestStrI(str, "Landlock")) {
                if (!InitLandlock(isolation, reveals))
                    return false;
            } else if (TestStrI(str, "Namespaces")) {
                if (!InitNamespaces(isolation, reveals))
                    return false;
            } else {
                LogError("Invalid sandbox method '%1'", str);
                return false;
            }
        } else {
            if (!InitLandlock(isolation, reveals)) {
                // Try namespaces to support older kernels.
                // Won't work inside Docker unless --cap_add=CAP_SYS_ADMIN is used.
                if (!InitNamespaces(isolation, reveals))
                    return false;
            }
        }
    }

    if (isolation & (int)sb_IsolationFlag::Syscalls) {
        if (!InitSeccomp(filters))
            return false;
    }

    return true;
}

}

#endif
