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
#define LANDLOCK_ABI_WARN 6
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

void sb_SandboxBuilder::RevealPaths(Span<const char *const> paths, bool readonly)
{
    for (const char *path: paths) {
        K_ASSERT(path[0] == '/');

        RevealedPath reveal = {};

        reveal.path = DuplicateString(TrimStrRight(path, K_PATH_SEPARATORS), &str_alloc).ptr;
        reveal.readonly = readonly;

        reveals.Append(reveal);
    }
}

void sb_SandboxBuilder::MaskFiles(Span<const char *const> filenames)
{
    for (const char *filename: filenames) {
        K_ASSERT(filename[0] == '/');

        const char *copy = DuplicateString(filename, &str_alloc).ptr;
        masks.Append(copy);
    }
}

void sb_SandboxBuilder::FilterSyscalls(Span<const sb_SyscallFilter> filters)
{
    for (sb_SyscallFilter filter: filters) {
        filter.name = DuplicateString(filter.name, &str_alloc).ptr;
        this->filters.Append(filter);
    }
}

static int InitLandlock(unsigned int flags, landlock_ruleset_attr *out_attr)
{
    landlock_ruleset_attr attr = {};

    attr.handled_access_fs = ACCESS_FS_READ | ACCESS_FS_WRITE;
    attr.handled_access_net = LANDLOCK_ACCESS_NET_BIND_TCP | LANDLOCK_ACCESS_NET_CONNECT_TCP;
    attr.scoped = LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET;

    if (flags & (int)sb_IsolationFlag::Signals) {
        attr.scoped |= LANDLOCK_SCOPE_SIGNAL;
    }

    int abi = syscall(__NR_landlock_create_ruleset, nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION);
    if (abi < 0) {
        LogError("Failed to use Landlock for sandboxing: %1", strerror(errno));
        return -1;
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

    if (abi < LANDLOCK_ABI_WARN) {
        LogWarning("Update the running kernel to leverage Landlock features provided by ABI version %1", LANDLOCK_ABI_WARN);
    }

    int fd = syscall(__NR_landlock_create_ruleset, &attr, K_SIZE(attr), 0);
    if (fd < 0) {
        LogError("Failed to create landlock ruleset: %1", strerror(errno));
        return -1;
    }

    *out_attr = attr;
    return fd;
}

bool sb_SandboxBuilder::Apply()
{
    K_ASSERT(isolation);

    // Drop all capabilities
    {
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
    }

    int landlock_fd = -1;
    struct landlock_ruleset_attr landlock_attr;
    K_DEFER { CloseDescriptor(landlock_fd); };

    // Prepare filesystem isolation
    if (isolation & (int)sb_IsolationFlag::Filesystem) {
        if (landlock_fd < 0) {
            landlock_fd = InitLandlock(isolation, &landlock_attr);
            if (landlock_fd < 0)
                return false;
        }

        for (const RevealedPath &reveal: reveals) {
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
            beneath.allowed_access &= landlock_attr.handled_access_fs;

            if (syscall(__NR_landlock_add_rule, landlock_fd, LANDLOCK_RULE_PATH_BENEATH, &beneath, 0) < 0) {
                LogError("Failed to add landlock rule for '%1': %2", reveal.path, strerror(errno));
                return false;
            }
        }
    }

    if (landlock_fd >= 0 && syscall(__NR_landlock_restrict_self, landlock_fd, 0) < 0) {
        LogError("Failed to apply landlock restrictions: %1", strerror(errno));
        return false;
    }

    // Install syscall filters
    if (isolation & (int)sb_IsolationFlag::Syscalls) {
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
