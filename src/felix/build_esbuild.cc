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
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/base/base.hh"
#include "build.hh"

namespace RG {

bool Builder::PrepareEsbuild()
{
    if (esbuild_binary)
        return true;

    // Try environment first
    {
        const char *str = GetEnv("ESBUILD_PATH");

        if (str && str[0]) {
            esbuild_binary = str;
            return true;
        }
    }

    // Try embedded builds
    {
#if defined(_WIN64)
        const char *binary = "vendor\\esbuild\\bin\\esbuild_windows_x64.exe";
#elif defined(__linux__) && defined(__x86_64__)
        const char *binary = "vendor/esbuild/bin/esbuild_linux_x64";
#elif defined(__linux__) && defined(__aarch64__)
        const char *binary = "vendor/esbuild/bin/esbuild_linux_arm64";
#elif defined(__APPLE__) && defined(__x86_64__)
        const char *binary = "vendor/esbuild/bin/esbuild_macos_x64";
#elif defined(__APPLE__) && defined(__aarch64__)
        const char *binary = "vendor/esbuild/bin/esbuild_macos_arm64";
#else
        const char *binary = nullptr;
#endif

        if (binary && TestFile(binary)) {
            esbuild_binary = binary;
            return true;
        }
    }

    // Build it if Go compiler is available
    {
#if defined(_WIN32)
        const char *binary = Fmt(&str_alloc, "%1%/esbuild.exe", shared_directory).ptr;
#else
        const char *binary = Fmt(&str_alloc, "%1%/esbuild", shared_directory).ptr;
#endif

        if (TestFile(binary)) {
            LocalArray<char, 128> build_version;
            LocalArray<char, 128> src_version;

            const char *version_cmd = Fmt(&str_alloc, "\"%1\" --version", binary).ptr;
            const char *version_txt = "vendor/esbuild/src/version.txt";

            build_version.len = ReadCommandOutput(version_cmd, build_version.data);
            src_version.len = ReadFile(version_txt, src_version.data);

            if (build_version.len >= 0 && TestStr(build_version, src_version)) {
                esbuild_binary = binary;
                return true;
            }
        }

        if (FindExecutableInPath("go")) {
            LogInfo("Building esbuild with Go compiler...");

            const char *cmd_line = Fmt(&str_alloc, "go build -o \"%1\" -buildvcs=false ./cmd/esbuild", binary).ptr;
            const char *work_dir = "vendor/esbuild/src";
            const char *gocache_dir = Fmt(&str_alloc, "%1/Go", shared_directory).ptr;

            ExecuteInfo::KeyValue variables[] = {
                { "GOCACHE", gocache_dir }
            };

            ExecuteInfo info = {};

            info.work_dir = work_dir;
            info.env_variables = variables;

            HeapArray<char> output_buf;
            int exit_code;
            bool started = ExecuteCommandLine(cmd_line, info, {}, Megabytes(4), &output_buf, &exit_code);

            if (!started) {
                return false;
            } else if (exit_code) {
                LogError("Failed to build esbuild %!..+(exit code %1)%!0", exit_code);
                StdErr->Write(output_buf);

                return false;
            }

            esbuild_binary = binary;
            return true;
        } else {
            LogError("Install Go compiler to build esbuild tool");
            return false;
        }
    }

    RG_UNREACHABLE();
}

static const char *MakeGlobalName(const char *filename, Allocator *alloc)
{
    Span<const char> basename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);
    Span<const char> name = SplitStr(basename, '.');

    Span<char> buf = AllocateSpan<char>(alloc, name.len + 1);

    for (Size i = 0; i < name.len; i++) {
        char c = name[i];
        buf[i] = IsAsciiAlphaOrDigit(c) ? c : '_';
    }
    buf[name.len] = 0;

    return buf.ptr;
}

const char *Builder::AddEsbuildSource(const SourceFileInfo &src)
{
    RG_ASSERT(src.type == SourceType::Esbuild);

    const char *meta_filename = build_map.FindValue({ current_ns, src.filename }, nullptr);

    // First, we need esbuild!
    if (!meta_filename && !PrepareEsbuild())
        return nullptr;

    // Build web bundle
    if (!meta_filename) {
        const char *bundle_filename = BuildObjectPath(src.filename, cache_directory, "", "");

        meta_filename = Fmt(&str_alloc, "%1.meta", bundle_filename).ptr;

        uint32_t features = build.features;
        features = src.target->CombineFeatures(features);
        features = src.CombineFeatures(features);

        Command cmd = {};

        // Assemble esbuild command
        {
            HeapArray<char> buf(&str_alloc);

            Fmt(&buf, "\"%1\" \"%2\" --bundle --log-level=warning", esbuild_binary, src.filename);
            if (features & (int)CompileFeature::ESM) {
                Fmt(&buf, " --format=esm");
            } else {
                const char *global_name = MakeGlobalName(src.filename, &str_alloc);
                Fmt(&buf, " --format=iife --global-name=%1", global_name);
            }
            Fmt(&buf, " --allow-overwrite --metafile=\"%1\" --outfile=\"%2\"", meta_filename, bundle_filename);

            if (features & (int)CompileFeature::Optimize) {
                Fmt(&buf, " --minify");
            }
            if (features & (int)CompileFeature::DebugInfo) {
                Fmt(&buf, " --sourcemap=linked");
            }
            if (src.target->bundle_options) {
                Fmt(&buf, " %1", src.target->bundle_options);
            }

            cmd.cache_len = buf.len;
            Fmt(&buf, " --color=%1", FileIsVt100(STDOUT_FILENO) ? "true" : "false");
            cmd.cmd_line = buf.TrimAndLeak(1);

            cmd.deps_mode = Command::DependencyMode::EsbuildMeta;
            cmd.deps_filename = meta_filename;
        }

        const char *text = Fmt(&str_alloc, "Bundle %!..+%1%!0", src.filename).ptr;
        if (AppendNode(text, meta_filename, cmd, { src.filename, esbuild_binary })) {
            if (!build.fake && !EnsureDirectoryExists(bundle_filename))
                return nullptr;
        }
    }

    return meta_filename;
}

}
