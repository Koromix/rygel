// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "instance.hh"
#include "vm.hh"
#include "lib/native/sandbox/sandbox.hh"
#include "lib/native/wrap/jscore.hh"
#include "lib/native/wrap/json.hh"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.h"
#include <poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

namespace K {

enum class RequestType {
    MergeData = 0
};

static const int64_t KillDelay = 5000;

static pid_t main_pid = 0;
static int main_pfd[2] = { -1, -1 };
static const char *main_directory = nullptr;

static JSGlobalContextRef vm_ctx;
static JSObjectRef vm_api;

static mz_zip_archive fs_zip;
static HashMap<const char *, Span<const uint8_t>> fs_map;
static BlockAllocator fs_alloc;

static bool ApplySandbox(Span<const char *const> reveal_paths)
{
    sb_SandboxBuilder sb;

    if (!sb.Init())
        return false;

    sb.RevealPaths(reveal_paths, true);

#if defined(__linux__)
    sb.FilterSyscalls({
        { "exit", sb_FilterAction::Allow },
        { "exit_group", sb_FilterAction::Allow },
        { "brk", sb_FilterAction::Allow },
        { "mmap", sb_FilterAction::Allow },
        { "munmap", sb_FilterAction::Allow },
        { "mremap", sb_FilterAction::Allow },
        { "mprotect/noexec", sb_FilterAction::Allow },
        { "mlock", sb_FilterAction::Allow },
        { "mlock2", sb_FilterAction::Allow },
        { "mlockall", sb_FilterAction::Allow },
        { "madvise", sb_FilterAction::Allow },
        { "pipe", sb_FilterAction::Allow },
        { "pipe2", sb_FilterAction::Allow },
        { "open", sb_FilterAction::Allow },
        { "openat", sb_FilterAction::Allow },
        { "openat2", sb_FilterAction::Allow },
        { "close", sb_FilterAction::Allow },
        { "fcntl", sb_FilterAction::Allow },
        { "read", sb_FilterAction::Allow },
        { "readv", sb_FilterAction::Allow },
        { "write", sb_FilterAction::Allow },
        { "writev", sb_FilterAction::Allow },
        { "pread64", sb_FilterAction::Allow },
        { "pwrite64", sb_FilterAction::Allow },
        { "lseek", sb_FilterAction::Allow },
        { "ftruncate", sb_FilterAction::Allow },
        { "fsync", sb_FilterAction::Allow },
        { "fdatasync", sb_FilterAction::Allow },
        { "fstat", sb_FilterAction::Allow },
        { "stat", sb_FilterAction::Allow },
        { "lstat", sb_FilterAction::Allow },
        { "lstat64", sb_FilterAction::Allow },
        { "fstatat64", sb_FilterAction::Allow },
        { "newfstatat", sb_FilterAction::Allow },
        { "statx", sb_FilterAction::Allow },
        { "access", sb_FilterAction::Allow },
        { "faccessat", sb_FilterAction::Allow },
        { "faccessat2", sb_FilterAction::Allow },
        { "ioctl/tty", sb_FilterAction::Allow },
        { "waitpid", sb_FilterAction::Allow },
        { "waitid", sb_FilterAction::Allow },
        { "wait3", sb_FilterAction::Allow },
        { "wait4", sb_FilterAction::Allow },
        { "getrandom", sb_FilterAction::Allow },
        { "getpid", sb_FilterAction::Allow },
        { "gettid", sb_FilterAction::Allow },
        { "getuid", sb_FilterAction::Allow },
        { "getgid", sb_FilterAction::Allow },
        { "geteuid", sb_FilterAction::Allow },
        { "getegid", sb_FilterAction::Allow },
        { "getcwd", sb_FilterAction::Allow },
        { "rt_sigaction", sb_FilterAction::Allow },
        { "rt_sigpending", sb_FilterAction::Allow },
        { "rt_sigprocmask", sb_FilterAction::Allow },
        { "rt_sigqueueinfo", sb_FilterAction::Allow },
        { "rt_sigreturn", sb_FilterAction::Allow },
        { "rt_sigsuspend", sb_FilterAction::Allow },
        { "rt_sigtimedwait", sb_FilterAction::Allow },
        { "rt_sigtimedwait_time64", sb_FilterAction::Allow },
        { "prlimit64", sb_FilterAction::Allow },
        { "sysinfo", sb_FilterAction::Allow },
        { "kill", sb_FilterAction::Allow },
        { "tgkill", sb_FilterAction::Allow },
        { "unlink", sb_FilterAction::Allow },
        { "unlinkat", sb_FilterAction::Allow },
        { "fork", sb_FilterAction::Allow },
        { "clone", sb_FilterAction::Allow },
        { "clone3", sb_FilterAction::Allow },
        { "futex", sb_FilterAction::Allow },
        { "futex_time64", sb_FilterAction::Allow },
        { "rseq", sb_FilterAction::Allow },
        { "set_robust_list", sb_FilterAction::Allow },
        { "getsockopt", sb_FilterAction::Allow },
        { "setsockopt", sb_FilterAction::Allow },
        { "getsockname", sb_FilterAction::Allow },
        { "getpeername", sb_FilterAction::Allow },
        { "getdents", sb_FilterAction::Allow },
        { "getdents64", sb_FilterAction::Allow },
        { "prctl", sb_FilterAction::Allow },
        { "poll", sb_FilterAction::Allow },
        { "ppoll", sb_FilterAction::Allow },
        { "select", sb_FilterAction::Allow },
        { "clock_nanosleep", sb_FilterAction::Allow },
        { "clock_gettime", sb_FilterAction::Allow },
        { "clock_gettime64", sb_FilterAction::Allow },
        { "clock_nanosleep", sb_FilterAction::Allow },
        { "clock_nanosleep_time64", sb_FilterAction::Allow },
        { "nanosleep", sb_FilterAction::Allow },
        { "sched_yield", sb_FilterAction::Allow },
        { "sched_getaffinity", sb_FilterAction::Allow },
        { "sched_getscheduler", sb_FilterAction::Allow },
        { "sched_setscheduler", sb_FilterAction::Allow },
        { "recv", sb_FilterAction::Allow },
        { "recvfrom", sb_FilterAction::Allow },
        { "recvmmsg", sb_FilterAction::Allow },
        { "recvmmsg_time64", sb_FilterAction::Allow },
        { "recvmsg", sb_FilterAction::Allow },
        { "sendmsg", sb_FilterAction::Allow },
        { "sendmmsg", sb_FilterAction::Allow },
        { "sendfile", sb_FilterAction::Allow },
        { "sendfile64", sb_FilterAction::Allow },
        { "sendto", sb_FilterAction::Allow },
        { "shutdown", sb_FilterAction::Allow },
        { "uname", sb_FilterAction::Allow },
        { "utime", sb_FilterAction::Allow },
        { "getrusage", sb_FilterAction::Allow },
        { "readlink", sb_FilterAction::Allow },
        { "readlinkat", sb_FilterAction::Allow }
    });
#endif

    return sb.Apply();
}

static bool InitView(const char *zip_filename)
{
    K_ASSERT(!fs_zip.m_pState);

    if (!mz_zip_reader_init_file(&fs_zip, zip_filename, 0)) {
        LogError("Failed to open ZIP archive '%1': %2", zip_filename, mz_zip_get_error_string(fs_zip.m_last_error));
        return false;
    }

    return true;
}

static void ReleaseView()
{
    mz_zip_reader_end(&fs_zip);

    fs_map.Clear();
    fs_alloc.ReleaseAll();
}

static bool LoadViewFile(const char *filename, Size max_len, Span<const uint8_t> *out_buf)
{
    K_ASSERT(fs_zip.m_pState);

    // Try the cache (fast path)
    {
        Span<const uint8_t> *ptr = fs_map.Find(filename);

        if (ptr) {
            *out_buf = *ptr;
            return true;
        }
    }

    int idx = mz_zip_reader_locate_file(&fs_zip, filename, nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
    Span<uint8_t> buf = {};

    if (idx >= 0) {
        mz_zip_archive_file_stat sb;
        if (!mz_zip_reader_file_stat(&fs_zip, (mz_uint)idx, &sb)) {
            LogError("Failed to stat '%1' in ZIP view: %2", filename, mz_zip_get_error_string(fs_zip.m_last_error));
            return false;
        }

        if (max_len >= 0 && sb.m_uncomp_size > (mz_uint64)max_len) {
            LogError("File '%1' is too big to handle (max = %2)", filename, FmtDiskSize(max_len));
            return false;
        }

        buf = AllocateSpan<uint8_t>(&fs_alloc, (Size)sb.m_uncomp_size);

        if (!mz_zip_reader_extract_to_mem(&fs_zip, (mz_uint)idx, buf.ptr, (size_t)buf.len, 0)) {
            LogError("Failed to extract '%1' from ZIP view: %2", filename, mz_zip_get_error_string(fs_zip.m_last_error));
            return false;
        }
    }

    fs_map.Set(filename, buf);

    *out_buf = buf;
    return true;
}

template <typename... Args>
JSObjectRef MakeError(JSContextRef ctx, const char *fmt, Args... args)
{
    char buf[2048];
    Fmt(buf, fmt, args...);

    js_AutoString str(buf);
    JSValueRef msg = JSValueMakeString(ctx, str);

    return JSObjectMakeError(ctx, 1, &msg, nullptr);
}

static JSValueRef GetFileData(JSContextRef ctx, JSObjectRef, JSObjectRef,
                              size_t argc, const JSValueRef argv[], JSValueRef *)
{
    if (argc < 1)
        return MakeError(ctx, "Expected 1 argument, got 0");
    if (!JSValueIsString(ctx, argv[0]))
        return MakeError(ctx, "Expected string argument");

    char filename[1024];
    {
        JSValueRef ex = nullptr;
        JSStringRef str = JSValueToStringCopy(ctx, argv[0], &ex);
        if (!str)
            return ex;
        K_DEFER { JSStringRelease(str); };

        JSStringGetUTF8CString(str, filename, K_SIZE(filename));
    }

    Span<const uint8_t> data;
    if (!LoadViewFile(filename, Mebibytes(4), &data))
        return MakeError(ctx, "Failed to get file '%1'", filename);

    JSStringRef str = JSStringCreateWithUTF8CStringWithLength((const char *)data.ptr, (size_t)data.len);
    JSValueRef value = JSValueMakeString(ctx, str);

    return value;
}

static void DumpException(JSContextRef ctx, JSValueRef ex)
{
    K_ASSERT(ex);

    js_PrintValue(ctx, ex, nullptr, StdErr);
    PrintLn(StdErr);
}

static JSValueRef CallMethod(JSContextRef ctx, JSObjectRef obj, const char *method,
                             Span<const JSValueRef> args)
{
    JSValueRef func = JSObjectGetProperty(ctx, obj, js_AutoString(method), nullptr);

    K_ASSERT(func);
    K_ASSERT(JSValueIsObject(ctx, func) && JSObjectIsFunction(ctx, (JSObjectRef)func));

    JSValueRef ex = nullptr;
    JSValueRef ret = JSObjectCallAsFunction(ctx, (JSObjectRef)func, obj, args.len, args.ptr, &ex);

    if (!ret) {
        DumpException(ctx, ex);
        return nullptr;
    }

    return ret;
}

static bool InitVM()
{
    // Get packed server script
    HeapArray<char> vm_js;
    {
        const AssetInfo *asset = FindEmbedAsset("src/goupile/server/vm.js");
        K_ASSERT(asset);

        StreamReader reader(asset->data, "<asset>", asset->compression_type);
        StreamWriter writer(&vm_js, "<memory>");

        if (!SpliceStream(&reader, -1, &writer))
            return false;
        K_ASSERT(writer.Close());
    }

    // Prepare VM for JS execution
    vm_ctx = JSGlobalContextCreate(nullptr);

    // Evaluate main script
    {
        JSValueRef ex = nullptr;
        JSValueRef ret = JSEvaluateScript(vm_ctx, js_AutoString(vm_js), nullptr, nullptr, 1, &ex);

        if (!ret) {
            DumpException(vm_ctx, ex);
            return false;
        }
    }

    // Prepare our protected native API
    JSValueRef native;
    {
        JSObjectRef obj = JSObjectMake(vm_ctx, nullptr, nullptr);

        js_ExposeFunction(vm_ctx, obj, "getFile", GetFileData);

        native = (JSValueRef)obj;
    }

    // Create API instance
    {
        JSObjectRef global = JSContextGetGlobalObject(vm_ctx);
        JSValueRef vm = JSObjectGetProperty(vm_ctx, global, js_AutoString("vm"), nullptr);
        JSValueRef construct = JSValueIsObject(vm_ctx, vm) ? JSObjectGetProperty(vm_ctx, (JSObjectRef)vm, js_AutoString("VmApi"), nullptr)
                                                           : JSValueMakeUndefined(vm_ctx);

        K_ASSERT(JSValueIsObject(vm_ctx, construct));
        K_ASSERT(JSObjectIsFunction(vm_ctx, (JSObjectRef)construct));

        JSValueRef args[] = {
            native
        };

        JSValueRef ex = nullptr;
        JSValueRef ret = JSObjectCallAsConstructor(vm_ctx, (JSObjectRef)construct, K_LEN(args), args, &ex);

        if (!ret) {
            DumpException(vm_ctx, ex);
            return false;
        }

        K_ASSERT(JSValueIsObject(vm_ctx, ret));
        vm_api = (JSObjectRef)ret;
    }

    return true;
}

static bool HandleMergeData(json_Parser *json, Allocator *alloc, StreamWriter *writer)
{
    Span<const char> data = {};
    Span<const char> meta = {};
    {
        for (json->ParseObject(); json->InObject(); ) {
            Span<const char> key = json->ParseKey();

            if (key == "data") {
                json->PassThrough(&data);
            } else if (key == "meta") {
                json->PassThrough(&meta);
            } else {
                LogError("Unexpected key '%1'", key);
                return false;
            }
        }
        if (!json->IsValid())
            return false;

        if (!data.len || !meta.len) {
            LogError("Missing merge values");
            return false;
        }
    }

    Span<const char> result;
    {
        JSValueRef args[] = {
            JSValueMakeString(vm_ctx, js_AutoString(data)),
            JSValueMakeString(vm_ctx, js_AutoString(meta))
        };

        JSValueRef ret = CallMethod(vm_ctx, vm_api, "mergeData", args);
        if (!ret)
            return false;
        K_ASSERT(JSValueIsString(vm_ctx, ret));

        result = js_ReadString(vm_ctx, ret, alloc);
    }

    writer->Write(result);

    return true;
}

static bool HandleRequest(int kind, struct cmsghdr *cmsg, pid_t *out_pid)
{
    BlockAllocator temp_alloc;

    if (!cmsg) {
        LogError("Missing ancillary data for request command");
        return false;
    }
    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS ||
                                          cmsg->cmsg_len != CMSG_LEN(K_SIZE(int))) {
        LogError("Missing socket descriptor for request command");
        return false;
    }

    int fd = -1;
    K_DEFER { CloseDescriptor(fd); };

    MemCpy(&fd, CMSG_DATA(cmsg), K_SIZE(int));

    pid_t pid = fork();

    if (pid < 0) {
        LogError("Failed to fork zygote process: %1", strerror(errno));
        return false;
    }

    // Not our problem anymore, let the forked process do the rest
    if (pid > 0) {
        *out_pid = pid;
        return true;
    }

    // Handle request
    {
        StreamReader reader(fd, "<server>");
        StreamWriter writer(fd, "<server>", (int)StreamWriterFlag::NoBuffer);
        json_Parser json(&reader, &temp_alloc);

        switch (kind) {
            case (int)RequestType::MergeData: { HandleMergeData(&json, &temp_alloc, &writer); } break;
            default: { LogError("Ignoring unknown message 0x%1 from server process", FmtHex(kind, 2)); } break;
        }

        if (!writer.Close())
            return false;

        shutdown(fd, SHUT_RDWR);
    }

    CloseDescriptor(fd);
    fd = -1;

    exit(0);
}

static bool DetectInterrupt()
{
    WaitResult ret = WaitEvents(0);

    if (ret == WaitResult::Exit)
        return true;
    if (ret == WaitResult::Interrupt)
        return true;

    return false;
}

static bool ServeRequests()
{
    struct RunningFork {
        pid_t pid;
        int64_t start;
    };

    HeapArray<RunningFork> forks;
    HeapArray<struct pollfd> pfds;

    pfds.Append({ main_pfd[1], POLLIN, 0 });

    while (!DetectInterrupt()) {
        uint8_t kind = 0;
        uint8_t control[256];

        struct iovec iov = { &kind, 1 };
        struct msghdr msg = {
            .msg_name = nullptr,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = control,
            .msg_controllen = K_SIZE(control),
            .msg_flags = 0
        };

        // Wait for request or fork timeout
        {
            int64_t now = GetMonotonicTime();

            unsigned int timeout = UINT_MAX;
            struct pollfd pfd = { main_pfd[1], POLLIN, 0 };

            for (const RunningFork &fork: forks) {
                int64_t duration = now - fork.start;
                int64_t delay = std::max(KillDelay - duration, (int64_t)1000);

                timeout = std::min(timeout, (unsigned int)delay);
            }

            if (poll(&pfd, 1, (int)timeout) < 0 && errno != EINTR) {
                LogError("Failed to poll in zygote process: %1", strerror(errno));
                return false;
            }
        }

        // Kill timed out forks
        {
            int64_t now = GetMonotonicTime();

            Size j = 0;
            for (Size i = 0; i < forks.len; i++) {
                RunningFork &fork = forks[i];

                forks[j] = forks[i];

                if (waitpid(fork.pid, nullptr, WNOHANG) > 0) {
                    LogDebug("VM %1 has ended", fork.pid);
                } else {
                    if (now - fork.start >= KillDelay) {
                        LogDebug("Kill VM %1 (timeout)", fork.pid);
                        kill(fork.pid, SIGKILL);
                    }

                    j++;
                }
            }
            forks.len = j;
        }

        Size ret = recvmsg(main_pfd[1], &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);

        if (ret < 0) {
            if (errno == EAGAIN)
                continue;

            LogError("Failed to read from UNIX socket: %1", strerror(errno));
            return false;
        }
        if (!ret)
            break;

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        RunningFork fork = {};

        if (!HandleRequest(kind, cmsg, &fork.pid))
            continue;
        fork.start = GetMonotonicTime();

        forks.Append(fork);
    }

    return true;
}

ZygoteResult RunZygote(bool sandbox, const char *view_directory)
{
    K_ASSERT(main_pfd[0] < 0);

    K_DEFER_N(pfd_guard) {
        CloseDescriptor(main_pfd[0]);
        CloseDescriptor(main_pfd[1]);

        main_pfd[0] = -1;
        main_pfd[1] = -1;
    };

    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, main_pfd) < 0) {
        LogError("Failed to create UNIX socket pair: %1", strerror(errno));
        return ZygoteResult::Error;
    }

    pid_t pid = fork();

    if (pid < 0) {
        LogError("Failed to run zygote process: %1", strerror(errno));
        return ZygoteResult::Error;
    }

    if (pid > 0) {
        CloseDescriptor(main_pfd[1]);
        main_pfd[1] = -1;

        main_pid = pid;

        uint8_t dummy = 0;
        Size ret = recv(main_pfd[0], &dummy, K_SIZE(dummy), 0);

        if (ret < 0) {
            LogError("Failed to read from zygote socket: %1", strerror(errno));
            return ZygoteResult::Error;
        } else if (!ret) {
            LogError("Zygote process failed to initialize");
            return ZygoteResult::Error;
        }

        atexit([]() { CloseDescriptor(main_pfd[0]); });
        pfd_guard.Disable();

        return ZygoteResult::Parent;
    } else {
        CloseDescriptor(main_pfd[0]);
        main_pfd[0] = -1;

        main_directory = view_directory;

        if (sandbox) {
            const char *const reveal_paths[] = {
                "/proc/self",
                "/dev/null",
                "/dev/random",
                "/dev/urandom",

                view_directory
            };

            if (!ApplySandbox(reveal_paths))
                return ZygoteResult::Error;
        }

        if (!InitVM())
            return ZygoteResult::Error;

        // I'm ready!
        uint8_t dummy = 0;
        Size ret = send(main_pfd[1], &dummy, K_SIZE(dummy), MSG_NOSIGNAL);

        if (ret < 0) {
            LogError("Failed to write to zygote socket: %1", strerror(errno));
            return ZygoteResult::Error;
        }
        K_ASSERT(ret);

        if (!ServeRequests())
            return ZygoteResult::Error;

        return ZygoteResult::Child;
    }
}

void StopZygote()
{
    if (main_pid <= 0)
        return;

    kill(main_pid, SIGTERM);

    // Terminate after delay
    {
        int64_t start = GetMonotonicTime();

        for (;;) {
            int ret = K_RESTART_EINTR(waitpid(main_pid, nullptr, WNOHANG), < 0);

            if (ret < 0) {
                LogError("Failed to wait for process exit: %1", strerror(errno));
                break;
            } else if (!ret) {
                int64_t delay = GetMonotonicTime() - start;

                if (delay < 2000) {
                    // A timeout on waitpid would be better, but... sigh
                    WaitDelay(10);
                } else {
                    kill(main_pid, SIGKILL);
                }
            } else {
                break;
            }
        }
    }
}

bool CheckZygote()
{
    K_ASSERT(main_pid > 0);

    int ret = waitpid(main_pid, nullptr, WNOHANG);

    if (ret < 0) {
        LogError("waitpid() call failed: %1", strerror(errno));
        return false;
    } else if (ret) {
        LogError("Zygote process has exited");

        main_pid = 0;
        return false;
    }

    return true;
}

static bool SendRequest(RequestType type, Allocator *alloc,
                        FunctionRef<bool(json_Writer *)> send, FunctionRef<bool(json_Parser *)> receive)
{
    K_ASSERT(alloc);
    K_ASSERT(main_pfd[0] >= 0);
    K_ASSERT(main_pfd[1] < 0);

    // Communicate through socket pair
    int pfd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pfd) < 0) {
        LogError("Failed to create UNIX socket pair: %1", strerror(errno));
        return false;
    }
    K_DEFER {
        CloseDescriptor(pfd[0]);
        CloseDescriptor(pfd[1]);
    };

    // Send request and connection to zygote
    {
        uint8_t kind = (uint8_t)type;
        uint8_t control[CMSG_SPACE(K_SIZE(int))] = {};

        struct iovec iov = { &kind, 1 };
        struct msghdr msg = {
            .msg_name = nullptr,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = control,
            .msg_controllen = K_SIZE(control),
            .msg_flags = 0
        };

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        K_ASSERT(cmsg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(K_SIZE(int));
        MemCpy(CMSG_DATA(cmsg), &pfd[1], K_SIZE(int));

        if (sendmsg(main_pfd[0], &msg, MSG_NOSIGNAL) < 0) {
            LogError("Failed to send run request to zygote: %1", strerror(errno));
            return false;
        }

        CloseDescriptor(pfd[1]);
        pfd[1] = -1;
    }

    // Send payload
    {
        StreamWriter st(pfd[0], "<zygote>");
        json_Writer json(&st);

        if (!send(&json))
            return false;
        if (!st.Close())
            return false;

        shutdown(pfd[0], SHUT_WR);
    }

    // Receive payload
    {
        StreamReader st(pfd[0], "<zygote>");
        json_Parser json(&st, alloc);

        if (!receive(&json))
            return false;
        if (!json.IsValid())
            return false;

        shutdown(pfd[0], SHUT_RD);
    }

    return true;
}

Span<const char> MergeData(Span<const char> data, Span<const char> meta, Allocator *alloc)
{
    BlockAllocator temp_alloc;

    if (!meta.len) {
        meta = "{}";
    }

    // Output
    Span<char> result = {};

    bool success = SendRequest(
        RequestType::MergeData, &temp_alloc,

        [&](json_Writer *json) {
            json->StartObject();
            json->Key("data"); json->Raw(data);
            json->Key("meta"); json->Raw(meta);
            json->EndObject();

            return true;
        },

        [&](json_Parser *json) {
            json->PassThrough(&result);
            return true;
        }
    );
    if (!success)
        return {};

    result = DuplicateString(result, alloc);
    return result;
}

}
