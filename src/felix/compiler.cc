// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#include "../libcc/libcc.hh"
#include "compiler.hh"

namespace RG {

static void AppendGccObjectArguments(const char *src_filename, BuildMode build_mode,
                                     const char *pch_filename, Span<const char *const> definitions,
                                     Span<const char *const> include_directories,
                                     const char *dest_filename, const char *deps_filename,
                                     HeapArray<char> *out_buf)
{
    if (LogUsesTerminalOutput()) {
        Fmt(out_buf, " -fdiagnostics-color=always");
    }

    switch (build_mode) {
        case BuildMode::Debug: { Fmt(out_buf, " -O0 -g -ftrapv"); } break;
        case BuildMode::Fast: { Fmt(out_buf, " -O2 -g -DNDEBUG"); } break;
        case BuildMode::Release: { Fmt(out_buf, " -O2 -flto -DNDEBUG"); } break;
    }

    Fmt(out_buf, " -c \"%1\"", src_filename);
    if (pch_filename) {
        Fmt(out_buf, " -include \"%1\"", pch_filename);
    }
    for (const char *definition: definitions) {
        Fmt(out_buf, " -D%1", definition);
    }
    for (const char *include_directory: include_directories) {
        Fmt(out_buf, " -I%1", include_directory);
    }
    if (deps_filename) {
        Fmt(out_buf, " -MMD -MF \"%1\"", deps_filename);
    }
    if (dest_filename) {
        Fmt(out_buf, " -o \"%1\"", dest_filename);
    }
}

#ifdef _WIN32
// FIXME: Response files should be handled further down the chain in a
// clean way, probably in RunBuildCommands().

static std::mutex rsp_files_mutex;
static BlockAllocator rsp_files_alloc;
static HeapArray<const char *> rsp_files;

RG_EXIT(DeleteResponseFiles)
{
    for (const char *filename: rsp_files) {
        unlink(filename);
    }
}

static const char *MakeTemporaryFile()
{
    WCHAR temp_path_w[1024];
    WCHAR temp_filename_w[1024];
    if (!GetTempPathW(RG_LEN(temp_path_w), temp_path_w))
        goto fail;
    if (!GetTempFileNameW(temp_path_w, L"fxb", 0, temp_filename_w))
        goto fail;

    // Convert to UTF-8
    char *filename;
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, temp_filename_w, -1, nullptr, 0,
                                       nullptr, nullptr);
        if (!size)
            goto fail;

        filename = (char *)Allocator::Allocate(&rsp_files_alloc, size);
        if (!WideCharToMultiByte(CP_UTF8, 0, temp_filename_w, -1,
                                 filename, size, nullptr, nullptr))
            goto fail;
    }

    // Add to cleanup list
    {
        std::lock_guard<std::mutex> lock(rsp_files_mutex);
        rsp_files.Append(filename);
    }

    return filename;

fail:
    LogError("Failed to create temporary filename");
    return nullptr;
}
#endif

static bool AppendGccLinkArguments(Span<const char *const> obj_filenames,
                                   Span<const char *const> libraries, const char *dest_filename,
                                   HeapArray<char> *out_buf)
{
    if (LogUsesTerminalOutput()) {
        Fmt(out_buf, " -fdiagnostics-color=always");
    }

#ifdef _WIN32
    Size rsp_offset = out_buf->len;

    for (const char *obj_filename: obj_filenames) {
        Fmt(out_buf, " \"%1\"", obj_filename);
    }

    if (out_buf->len - rsp_offset >= 4096) {
        const char *rsp_filename = MakeTemporaryFile();
        if (!rsp_filename)
            return false;

        // Apparently backslash characters needs to be escaped in response files,
        // but it's easier to use '/' instead.
        Span<char> arguments = out_buf->Take(rsp_offset + 1, out_buf->len - rsp_offset - 1);
        for (Size i = 0; i < arguments.len; i++) {
            arguments[i] = (arguments[i] == '\\' ? '/' : arguments[i]);
        }
        if (!WriteFile(arguments, rsp_filename))
            return false;

        out_buf->RemoveFrom(rsp_offset);
        Fmt(out_buf, " \"@%1\"", rsp_filename);
    }
#else
    for (const char *obj_filename: obj_filenames) {
        Fmt(out_buf, " \"%1\"", obj_filename);
    }
#endif

    for (const char *lib: libraries) {
        Fmt(out_buf, " -l%1", lib);
    }

    Fmt(out_buf, " -o \"%1\"", dest_filename);

    return true;
}

static void AppendPackCommandLine(Span<const char *const> pack_filenames, BuildMode build_mode,
                                  const char *pack_options, HeapArray<char> *out_buf)
{
#ifdef _WIN32
    // For some reason quotes fail here (maybe we need to escape properly), so use short file name
    Fmt(out_buf, "cmd /c %1 pack", GetApplicationExecutable83());
#else
    Fmt(out_buf, "\"%1\" pack", GetApplicationExecutable());
#endif

    switch (build_mode) {
        case BuildMode::Debug:
        case BuildMode::Fast: { Fmt(out_buf, " -m SourceMap"); } break;
        case BuildMode::Release: { Fmt(out_buf, " -m RunTransform"); } break;
    }

    if (pack_options) {
        Fmt(out_buf, " %1", pack_options);
    }
    for (const char *pack_filename: pack_filenames) {
        Fmt(out_buf, " \"%1\"", pack_filename);
    }
}

class ClangCompiler: public Compiler {
public:
    ClangCompiler(const char *name, const char *prefix) : Compiler(name, prefix) {}

    const char *MakeObjectCommand(const char *src_filename, SourceType src_type, BuildMode build_mode,
                                  bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                  Span<const char *const> include_directories, const char *dest_filename,
                                  const char *deps_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        // Compiler
        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "%1clang -std=gnu11", prefix); } break;
            case SourceType::C_Header: { Fmt(&buf, "%1clang -std=gnu11 -x c-header", prefix); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "%1clang++ -std=gnu++17", prefix); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "%1clang++ -std=gnu++17 -x c++-header", prefix); } break;
        }
        Fmt(&buf, warnings ? " -Wall" : " -Wno-everything");

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -D_MT -Xclang --dependent-lib=libcmt -Xclang --dependent-lib=oldnames"
                  " -Wno-unknown-warning-option -Wno-unknown-pragmas -Wno-deprecated-declarations"
                  " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DNOMINMAX"
                  " -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE");

        if (src_type == SourceType::CXX_Source || src_type == SourceType::CXX_Header) {
            Fmt(&buf, " -Xclang -flto-visibility-public-std");
        }
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC");
#else
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2"
                  " -pthread -fPIC -fstack-protector-strong --param ssp-buffer-size=4");
#endif

        // Common flags (source, definitions, include directories, etc.)
        AppendGccObjectArguments(src_filename, build_mode, pch_filename, definitions,
                                 include_directories, dest_filename, deps_filename, &buf);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakePackCommand(Span<const char *const> pack_filenames, BuildMode build_mode,
                                const char *pack_options, const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        AppendPackCommandLine(pack_filenames, build_mode, pack_options, &buf);
        Fmt(&buf, " | %1clang -x c -c - -o \"%2\"", prefix, dest_filename);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakeLinkCommand(Span<const char *const> obj_filenames, BuildMode build_mode,
                                Span<const char *const> libraries, LinkType link_type,
                                const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "%1clang++", prefix); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "%1clang++ -shared", prefix); } break;
        }

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -fuse-ld=lld");
#elif defined(__APPLE__)
        Fmt(&buf, " -ldl -pthread");
#else
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now");
#endif

        // Build mode
        if (build_mode == BuildMode::Release) {
            Fmt(&buf, " -flto");
            if (link_type == LinkType::Executable) {
                Fmt(&buf, " -static");
            }
        } else {
            Fmt(&buf, " -g");
        }

        // Objects and libraries
        if (!AppendGccLinkArguments(obj_filenames, libraries, dest_filename, &buf))
            return (const char *)nullptr;

        return (const char *)buf.Leak().ptr;
    }
};

class GnuCompiler: public Compiler {
public:
    GnuCompiler(const char *name, const char *prefix) : Compiler(name, prefix) {}

    const char *MakeObjectCommand(const char *src_filename, SourceType src_type, BuildMode build_mode,
                                  bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                  Span<const char *const> include_directories, const char *dest_filename,
                                  const char *deps_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        // Compiler
        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "%1gcc -std=gnu11", prefix); } break;
            case SourceType::C_Header: { Fmt(&buf, "%1gcc -std=gnu11 -x c-header", prefix); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "%1g++ -std=gnu++17 -fno-exceptions", prefix); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "%1g++ -std=gnu++17 -fno-exceptions -x c++-header", prefix); } break;
        }
        if (warnings) {
            Fmt(&buf, " -Wall");
            if (src_type == SourceType::CXX_Source || src_type == SourceType::CXX_Header) {
                Fmt(&buf, " -Wno-class-memaccess");
            }
        }

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2"
                  " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -D__USE_MINGW_ANSI_STDIO=1");
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC");
#else
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2"
                  " -pthread -fPIC -fstack-protector-strong --param ssp-buffer-size=4");
#endif

        // Common flags (source, definitions, include directories, etc.)
        AppendGccObjectArguments(src_filename, build_mode, pch_filename, definitions,
                                 include_directories, dest_filename, deps_filename, &buf);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakePackCommand(Span<const char *const> pack_filenames, BuildMode build_mode,
                                const char *pack_options, const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        AppendPackCommandLine(pack_filenames, build_mode, pack_options, &buf);
        Fmt(&buf, " | %1gcc -x c -c - -o \"%2\"", prefix, dest_filename);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakeLinkCommand(Span<const char *const> obj_filenames, BuildMode build_mode,
                                Span<const char *const> libraries, LinkType link_type,
                                const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "%1g++", prefix); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "%1g++ -shared", prefix); } break;
        }

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va");
#elif defined(__APPLE__)
        Fmt(&buf, " -ldl -pthread");
#else
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
#endif

        // Build mode
        if (build_mode == BuildMode::Release) {
            Fmt(&buf, " -flto -s");
            if (link_type == LinkType::Executable) {
                Fmt(&buf, " -static");
            }
        } else {
            Fmt(&buf, " -g");
        }

        // Objects and libraries
        if (!AppendGccLinkArguments(obj_filenames, libraries, dest_filename, &buf))
            return (const char *)nullptr;

        return (const char *)buf.Leak().ptr;
    }
};

static ClangCompiler ClangCompiler("Clang", "");
static GnuCompiler GnuCompiler("GCC", "");

static const Compiler *const CompilerTable[] = {
    &ClangCompiler,
    &GnuCompiler
};
const Span<const Compiler *const> Compilers = CompilerTable;

}
