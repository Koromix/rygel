// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef __linux__

#include "../libcc/libcc.hh"
#include "sandbox.hh"

#include <fcntl.h>
#include <sched.h>
#include <seccomp.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <termios.h>
#include <unistd.h>

// For some reason sys/capability.h is in some crap separate package, because why make
// it simple when you could instead make it a mess and require users to install
// random distribution-specifc packages to do anything of value.
#define _LINUX_CAPABILITY_VERSION_3  0x20080522
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

bool sb_IsSandboxSupported()
{
    return true;
}

sb_SandboxBuilder::~sb_SandboxBuilder()
{
    if (seccomp_ctx) {
        seccomp_release(seccomp_ctx);
    }
}

void sb_SandboxBuilder::IsolateProcess()
{
    unshare_flags |= CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWIPC | CLONE_NEWUTS |
                     CLONE_NEWNET | CLONE_NEWPID | CLONE_NEWCGROUP;
}

void sb_SandboxBuilder::MountPath(const char *src, const char *dest, bool readonly)
{
    RG_ASSERT(unshare_flags & CLONE_NEWNS);
    RG_ASSERT(src[0] == '/');
    RG_ASSERT(dest[0] == '/');

    BindMount bind = {};
    bind.src = DuplicateString(src, &alloc).ptr;
    bind.dest = DuplicateString(dest, &alloc).ptr;
    bind.readonly = readonly;
    mounts.Append(bind);
}

bool sb_SandboxBuilder::InitSyscallFilter(sb_SyscallAction default_action)
{
    RG_ASSERT(!seccomp_ctx);

    if (prctl(PR_GET_SECCOMP, 0, 0, 0, 0) < 0) {
        LogError("Cannot sandbox syscalls: seccomp is not available");
        return false;
    }

    // Check support for KILL_PROCESS action
    {
        uint32_t action = SCMP_ACT_KILL_PROCESS;
        if (!syscall(__NR_seccomp, 2, 0, &action)) { // SECCOMP_GET_ACTION_AVAIL
            seccomp_kill_action = SCMP_ACT_KILL_PROCESS;
        } else {
            LogError("Seccomp action KILL_PROCESS is not available; falling back to KILL_THREAD");
            seccomp_kill_action = SCMP_ACT_KILL_THREAD;
        }
    }

    seccomp_ctx = seccomp_init(TranslateAction(default_action));
    if (!seccomp_ctx) {
        LogError("Cannot sandbox syscalls: seccomp_init() failed");
        return false;
    }
    this->default_action = default_action;

    return true;
}

bool sb_SandboxBuilder::FilterSyscalls(sb_SyscallAction action, Span<const char *const> names)
{
    RG_ASSERT(seccomp_ctx);

    for (const char *name: names) {
        if (action != default_action) {
            int ret = 0;

            if (TestStr(name, "ioctl/tty")) {
                int syscall = seccomp_syscall_resolve_name("ioctl");
                RG_ASSERT(syscall != __NR_SCMP_ERROR);

#ifdef RG_ARCH_64
                ret = seccomp_rule_add(seccomp_ctx, TranslateAction(action), syscall, 1,
                                       SCMP_A1_64(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFFFFFFF00ul, 0x5400u));
#else
                ret = seccomp_rule_add(seccomp_ctx, TranslateAction(action), syscall, 1,
                                       SCMP_A1_32(SCMP_CMP_MASKED_EQ, 0xFFFFFF00ul, 0x5400u));
#endif
            } else {
                int syscall = seccomp_syscall_resolve_name(name);

                if (syscall != __NR_SCMP_ERROR) {
                    if (!filtered_syscalls.TrySet(syscall).second) {
                        LogError("Duplicate syscall filter for '%1'", name);
                        return false;
                    }

                    ret = seccomp_rule_add(seccomp_ctx, TranslateAction(action), syscall, 0);
                } else {
                    if (strchr(name, '/')) {
                        LogError("Unknown syscall specifier '%1'", name);
                        return false;
                    } else {
                        LogError("Ignoring unknown syscall '%1'", name);
                    }
                }
            }

            if (ret < 0) {
                LogError("Invalid seccomp syscall '%1': %2", name, strerror(-ret));
                return false;
            }
        }
    }

    return true;
}

void sb_SandboxBuilder::DropCapabilities()
{
    drop_caps = true;
}

bool sb_SandboxBuilder::Apply()
{
    uid_t uid = getuid();
    gid_t gid = getgid();

    if (!uid) {
        LogError("Refusing to sandbox as root");
        return false;
    }

    // Start namespacing
    if (unshare_flags && unshare(unshare_flags) < 0) {
        LogError("Failed to create namespace: %1", strerror(errno));
        return false;
    }

    // Set up user namespace
    if (unshare_flags & CLONE_NEWUSER) {
        int uid_fd = open("/proc/self/uid_map", O_CLOEXEC | O_WRONLY);
        if (uid_fd < 0) {
            LogError("Failed to open '/proc/self/uid_map' for writing: %1", strerror(errno));
            return false;
        }
        RG_DEFER { close(uid_fd); };

        int gid_fd = open("/proc/self/gid_map", O_CLOEXEC | O_WRONLY);
        if (gid_fd < 0) {
            LogError("Failed to open '/proc/self/gid_map' for writing: %1", strerror(errno));
            return false;
        }
        RG_DEFER { close(gid_fd); };

        LocalArray<char, 512> buf;

        // Write UID map
        buf.len = Fmt(buf.data, "%1 %1 1\n", uid).len;
        if (write(uid_fd, buf.data, buf.len) < 0) {
            LogError("Failed to write UID map: %1", strerror(errno));
            return false;
        }

        // More random crap Linux wants us to do, or writing GID map will fail
        if (!WriteFile("deny", "/proc/self/setgroups"))
            return false;

        // Write GID map
        buf.len = Fmt(buf.data, "%1 %1 1\n", gid).len;
        if (write(gid_fd, buf.data, buf.len) < 0) {
            LogError("Failed to write GID map: %1", strerror(errno));
            return false;
        }
    }

    // Set up FS namespace
    if (unshare_flags & CLONE_NEWNS) {
        if (!MakeDirectory("/tmp/sandbox", false))
            return false;
        if (mount("tmpfs", "/tmp/sandbox", "tmpfs", 0, "size=4k") < 0 && errno != EBUSY) {
            LogError("Failed to mount tmpfs on '/tmp/sandbox': %2", strerror(errno));
            return false;
        }
        if (mount(nullptr, "/tmp/sandbox", nullptr, MS_PRIVATE, nullptr) < 0) {
            LogError("Failed to set MS_PRIVATE on '/tmp/sandbox': %1", strerror(errno));
            return false;
        }

        // Create root FS with tmpfs
        const char *fs_root = CreateTemporaryDirectory("/tmp/sandbox", "", &alloc);
        if (!fs_root)
            return false;
        if (mount("tmpfs", fs_root, "tmpfs", 0, "size=4k") < 0) {
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
            const char *dest = Fmt(&alloc, "%1%2", fs_root, bind.dest).ptr;
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
        if (mount(nullptr, fs_root, nullptr, MS_REMOUNT, "size=1M,mode=0700,ro") < 0) {
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

    // Drop all capabilities
    if (drop_caps) {
        LogDebug("Dropping all capabilities");

        // PR_CAPBSET_DROP is thread-specific, so hopefully the sandbox is run before any thread
        // has been created. Who designs this crap??
        for (int i = 0; i < 64; i++) {
            if (prctl(PR_CAPBSET_DROP, i, 0, 0, 0) < 0 && errno != EINVAL) {
                LogError("Failed to drop bounding capability set: %1", strerror(errno));
                return false;
            }
        }

        cap_user_header hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
        cap_user_data data[2];
        memset(data, 0, RG_SIZE(data));

        if (syscall(__NR_capset, &hdr, data) < 0) {
            LogError("Failed top drop capabilities: %1", strerror(errno));
            return false;
        }
    }

    // Install syscall filters
    if (seccomp_ctx) {
        LogDebug("Applying syscall filters");

        int ret = seccomp_load(seccomp_ctx);
        if (ret < 0) {
            LogError("Failed to install syscall filters: %1", strerror(-ret));
            return false;
        }
    }

    return true;
}

uint32_t sb_SandboxBuilder::TranslateAction(sb_SyscallAction action) const
{
    uint32_t seccomp_action = UINT32_MAX;
    switch (action) {
        case sb_SyscallAction::Allow: { seccomp_action = SCMP_ACT_ALLOW; } break;
        case sb_SyscallAction::Log: { seccomp_action = SCMP_ACT_LOG; } break;
        case sb_SyscallAction::Block: { seccomp_action = SCMP_ACT_ERRNO(EPERM); } break;
        case sb_SyscallAction::Trap: { seccomp_action = SCMP_ACT_TRAP; } break;
        case sb_SyscallAction::Kill: { seccomp_action = seccomp_kill_action; } break;
    }
    RG_ASSERT(seccomp_action != UINT32_MAX);

    return seccomp_action;
}

}

#endif
