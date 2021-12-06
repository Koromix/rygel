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

#ifdef __linux__

#include "../libcc/libcc.hh"
#include "sandbox.hh"

#include <fcntl.h>
#include <sched.h>
#include "../../../vendor/libseccomp/include/seccomp.h"
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
#include <unistd.h>

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

namespace RG {

bool sec_IsSandboxSupported()
{
#if defined(__SANITIZE_ADDRESS__)
    LogError("Sandboxing does not support AddressSanitizer");
    return false;
#elif defined(__SANITIZE_THREAD__)
    LogError("Sandboxing does not support ThreadSanitizer");
    return false;
#else
    return true;
#endif
}

void sec_SandboxBuilder::RevealPaths(Span<const char *const> paths, bool readonly)
{
    for (const char *path: paths) {
        MountPath(path, path, readonly);
    }
}

void sec_SandboxBuilder::MountPath(const char *src, const char *dest, bool readonly)
{
    RG_ASSERT(src[0] == '/');
    RG_ASSERT(dest[0] == '/' && dest[1]);

    BindMount bind = {};
    bind.src = NormalizePath(src, &str_alloc).ptr;
    bind.dest = NormalizePath(dest, "/", &str_alloc).ptr;
    bind.readonly = readonly;
    mounts.Append(bind);
}

void sec_SandboxBuilder::FilterSyscalls(sec_FilterAction default_action, Span<const sec_FilterItem> items)
{
    RG_ASSERT(!filter_syscalls);

    filter_syscalls = true;
    this->default_action = default_action;

    FilterSyscalls(items);
}

void sec_SandboxBuilder::FilterSyscalls(Span<const sec_FilterItem> items)
{
    RG_ASSERT(filter_syscalls);

    filter_items.Reserve(items.len);
    for (sec_FilterItem item: items) {
        item.name = DuplicateString(item.name, &str_alloc).ptr;
        filter_items.Append(item);
    }
}

static bool WriteUidGidMap(pid_t pid, uid_t uid, gid_t gid)
{
    int uid_fd;
    {
        char buf[512];
        Fmt(buf, "/proc/%1/uid_map", pid);

        uid_fd = open(buf, O_CLOEXEC | O_WRONLY);
        if (uid_fd < 0) {
            LogError("Failed to open '%1' for writing: %2", buf, strerror(errno));
            return false;
        }
    }
    RG_DEFER { close(uid_fd); };

    int gid_fd;
    {
        char buf[512];
        Fmt(buf, "/proc/%1/gid_map", pid);

        gid_fd = open(buf, O_CLOEXEC | O_WRONLY);
        if (gid_fd < 0) {
            LogError("Failed to open '%1' for writing: %2", buf, strerror(errno));
            return false;
        }
    }
    RG_DEFER { close(gid_fd); };

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
        if (RG_POSIX_RESTART_EINTR(write(uid_fd, buf.data, buf.len), < 0) < 0) {
            LogError("Failed to write UID map: %1", strerror(errno));
            return false;
        }
    }

    // Write GID map
    {
        LocalArray<char, 512> buf;

        buf.len = Fmt(buf.data, "%1 %1 1\n", gid).len;
        if (RG_POSIX_RESTART_EINTR(write(gid_fd, buf.data, buf.len), < 0) < 0) {
            LogError("Failed to write GID map: %1", strerror(errno));
            return false;
        }
    }

    return true;
}

static bool InitNamespaces()
{
    uint32_t unshare_flags = CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWIPC |
                             CLONE_NEWUTS | CLONE_NEWCGROUP | CLONE_NEWNET |
                             CLONE_THREAD;

    if (unshare(unshare_flags) < 0) {
        LogError("Failed to create namespace: %1", strerror(errno));
        return false;
    }

    return true;
}

bool sec_SandboxBuilder::Apply()
{
    uid_t uid = getuid();
    gid_t gid = getgid();

    if (!uid) {
        LogError("Refusing to sandbox as root");
        return false;
    }

    // Start new namespace
    {
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
            RG_DEFER { close(efd); };

            pid_t child_pid = fork();
            if (child_pid < 0) {
                LogError("Failed to fork: %1", strerror(errno));
                return false;
            }

            if (child_pid) {
                RG_DEFER_N(kill_guard) {
                    kill(child_pid, SIGKILL);
                    waitpid(child_pid, nullptr, 0);
                };

                // This allows the sandbox helper to write to our /proc files even when
                // running as non-root in the CAP_SYS_ADMIN sandbox path.
                prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);

                int64_t dummy = 1;
                if (!InitNamespaces())
                    return false;
                if (RG_POSIX_RESTART_EINTR(write(efd, &dummy, RG_SIZE(dummy)), < 0) < 0) {
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

                // Set non-root container UID and GID
                if (setresuid(uid, uid, uid) < 0 || setresgid(gid, gid, gid) < 0) {
                    LogError("Cannot change UID or GID: %1", strerror(errno));
                    return false;
                }

                if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) < 0) {
                    LogError("Failed to clear dumpable proc attribute: %1", strerror(errno));
                    return false;
                }
            } else {
                int64_t dummy;
                if (RG_POSIX_RESTART_EINTR(read(efd, &dummy, RG_SIZE(dummy)), < 0) < 0) {
                    LogError("Failed to read eventfd: %1", strerror(errno));
                    _exit(1);
                }

                bool success = WriteUidGidMap(getppid(), uid, gid);
                _exit(!success);
            }
        } else {
            LogDebug("Trying unprivileged sandbox method");

            if (!InitNamespaces())
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
            const char *fs_root = CreateTemporaryDirectory("/tmp/sandbox", "", &str_alloc);
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
            for (const BindMount &bind: mounts) {
                const char *dest = Fmt(&str_alloc, "%1%2", fs_root, bind.dest).ptr;
                int flags = MS_BIND | MS_REC | (bind.readonly ? MS_RDONLY : 0);

                // Ensure destination exists
                {
                    FileInfo src_info;
                    if (!StatFile(bind.src, &src_info))
                        return false;

                    if (src_info.type == FileType::Directory) {
                        if (!MakeDirectoryRec(dest))
                            return false;
                    } else {
                        if (!EnsureDirectoryExists(dest))
                            return false;

                        FILE *fp = OpenFile(dest, (int)OpenFileFlag::Write);
                        if (!fp)
                            return false;
                        fclose(fp);
                    }
                }

                if (mount(bind.src, dest, nullptr, flags, nullptr) < 0) {
                    LogError("Failed to mount '%1' to '%2': %3", bind.src, dest, strerror(errno));
                    return false;
                }
            }

            // Remount root FS as readonly
            if (mount(nullptr, fs_root, nullptr, MS_REMOUNT | MS_RDONLY, nullptr) < 0) {
                LogError("Failed to set sandbox root to readonly");
                return false;
            }

            // Do the silly pivot_root dance
            {
                int old_root_fd = open("/", O_DIRECTORY | O_PATH);
                if (old_root_fd < 0) {
                    LogError("Failed to open directory '/': %1", strerror(errno));
                    return false;
                }
                RG_DEFER { close(old_root_fd); };

                int new_root_fd = open(fs_root, O_DIRECTORY | O_PATH);
                if (new_root_fd < 0) {
                    LogError("Failed to open directory '%1': %2", fs_root, strerror(errno));
                    return false;
                }
                RG_DEFER { close(new_root_fd); };

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
                    LogError("Failed to set MS_PRIVATE on ", fs_root, strerror(errno));
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
    }

    // Drop all capabilities
    {
        LogDebug("Dropping all capabilities");

        // PR_CAPBSET_DROP is thread-specific, so hopefully the sandbox is run before any thread
        // has been created. Who designs this crap??
        for (int i = 0; i < 64; i++) {
            if (prctl(PR_CAPBSET_DROP, i, 0, 0, 0) < 0 && errno != EINVAL) {
                LogError("Failed to drop bounding capability set: %1", strerror(errno));
                return false;
            }
        }

        // This is recent (Linux 4.3), so ignore EINVAL
        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) < 0 && errno != EINVAL) {
            LogError("Failed to clear ambient capability set: %1", strerror(errno));
            return false;
        }

        cap_user_header hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
        cap_user_data data[2];
        memset_safe(data, 0, RG_SIZE(data));

        if (syscall(__NR_capset, &hdr, data) < 0) {
            LogError("Failed to drop capabilities: %1", strerror(errno));
            return false;
        }

        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
            LogError("Failed to restrict privileges: %1", strerror(errno));
            return false;
        }
    }

    // Install syscall filters
    if (filter_syscalls) {
        LogDebug("Applying syscall filters");

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

        const auto translate_action = [&](sec_FilterAction action) {
            uint32_t seccomp_action = UINT32_MAX;
            switch (action) {
                case sec_FilterAction::Allow: { seccomp_action = SCMP_ACT_ALLOW; } break;
                case sec_FilterAction::Block: { seccomp_action = SCMP_ACT_ERRNO(EPERM); } break;
                case sec_FilterAction::Trap: { seccomp_action = SCMP_ACT_TRAP; } break;
                case sec_FilterAction::Kill: { seccomp_action = kill_code; } break;
            }
            RG_ASSERT(seccomp_action != UINT32_MAX);

            return seccomp_action;
        };

        scmp_filter_ctx ctx = seccomp_init(translate_action(default_action));
        if (!ctx) {
            LogError("Cannot sandbox syscalls: seccomp_init() failed");
            return false;
        }
        RG_DEFER { seccomp_release(ctx); };

        for (const sec_FilterItem &item: filter_items) {
            if (item.action != default_action) {
                int ret = 0;

                if (TestStr(item.name, "ioctl/tty")) {
                    int syscall = seccomp_syscall_resolve_name("ioctl");
                    RG_ASSERT(syscall != __NR_SCMP_ERROR);

                    ret = seccomp_rule_add(ctx, translate_action(item.action), syscall, 1,
                                           SCMP_A1(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFFFFFFF00ul, 0x5400u));
                } else if (TestStr(item.name, "mmap/anon")) {
                    int syscall = seccomp_syscall_resolve_name("mmap");
                    RG_ASSERT(syscall != __NR_SCMP_ERROR);

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
                            ret = seccomp_rule_add(ctx, translate_action(item.action), syscall, 3,
                                                   SCMP_A2(SCMP_CMP_MASKED_EQ, prot_mask, prot_flags),
                                                   SCMP_A3(SCMP_CMP_EQ, map_flags),
                                                   SCMP_A4(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, 0xFFFFFFFFu));
                            if (ret < 0)
                                break;
                        }
                    }
                } else if (TestStr(item.name, "mmap/shared")) {
                    int syscall = seccomp_syscall_resolve_name("mmap");
                    RG_ASSERT(syscall != __NR_SCMP_ERROR);

                    unsigned int prot_mask = PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC;
                    unsigned int prot_combinations[] = {
                        PROT_NONE,
                        PROT_READ,
                        PROT_WRITE,
                        PROT_READ | PROT_WRITE
                    };

                    for (unsigned int prot_flags: prot_combinations) {
                        ret = seccomp_rule_add(ctx, translate_action(item.action), syscall, 2,
                                               SCMP_A2(SCMP_CMP_MASKED_EQ, prot_mask, prot_flags),
                                               SCMP_A3(SCMP_CMP_EQ, MAP_SHARED));
                        if (ret < 0)
                            break;
                    }
                } else if (TestStr(item.name, "mprotect/noexec")) {
                    int syscall = seccomp_syscall_resolve_name("mprotect");
                    RG_ASSERT(syscall != __NR_SCMP_ERROR);

                    unsigned int prot_mask = PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC;
                    unsigned int prot_combinations[] = {
                        PROT_NONE,
                        PROT_READ,
                        PROT_WRITE,
                        PROT_READ | PROT_WRITE
                    };

                    for (unsigned int prot_flags: prot_combinations) {
                        ret = seccomp_rule_add(ctx, translate_action(item.action), syscall, 1,
                                               SCMP_A2(SCMP_CMP_MASKED_EQ, prot_mask, prot_flags));
                        if (ret < 0)
                            break;
                    }
                } else if (TestStr(item.name, "clone/thread")) {
                    int syscall = seccomp_syscall_resolve_name("clone");
                    RG_ASSERT(syscall != __NR_SCMP_ERROR);

                    unsigned int flags = CLONE_THREAD | CLONE_SIGHAND | CLONE_VM;
                    ret = seccomp_rule_add(ctx, translate_action(item.action), syscall, 1,
                                           SCMP_A0(SCMP_CMP_MASKED_EQ, flags, flags));
                } else {
                    int syscall = seccomp_syscall_resolve_name(item.name);

                    if (syscall != __NR_SCMP_ERROR) {
                        ret = seccomp_rule_add(ctx, translate_action(item.action), syscall, 0);
                    } else {
                        if (strchr(item.name, '/')) {
                            LogError("Unknown syscall specifier '%1'", item.name);
                            return false;
                        } else {
                            LogDebug("Ignoring unknown syscall '%1'", item.name);
                        }
                    }
                }

                if (ret < 0) {
                    LogError("Invalid seccomp syscall '%1': %2", item.name, strerror(-ret));
                    return false;
                }
            }
        }

        int ret = seccomp_load(ctx);
        if (ret < 0) {
            LogError("Failed to install syscall filters: %1", strerror(-ret));
            return false;
        }
    }

    return true;
}

}

#endif
