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

#include "src/core/libcc/libcc.hh"
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace RG {

void FindArduinoCompiler(const char *name, const char *compiler, Span<char> out_cc)
{
#ifdef _WIN32
    wchar_t buf[2048];
    DWORD buf_len = RG_LEN(buf);

    bool arduino = !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len) ||
                   !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len) ||
                   !RegGetValueW(HKEY_CURRENT_USER, L"Software\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len) ||
                   !RegGetValueW(HKEY_CURRENT_USER, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len);
    if (!arduino)
        return;

    Size pos = ConvertWin32WideToUtf8(buf, out_cc);
    if (pos < 0)
        return;

    Span<char> remain = out_cc.Take(pos, out_cc.len - pos);
    Fmt(remain, "%/%1.exe", compiler);
    for (Size i = 0; remain.ptr[i]; i++) {
        char c = remain.ptr[i];
        remain.ptr[i] = (c == '/') ? '\\' : c;
    }

    if (TestFile(out_cc.ptr, FileType::File)) {
        LogDebug("Found %1 compiler for Teensy: '%2'", name, out_cc.ptr);
    } else {
        out_cc[0] = 0;
    }
#else
    struct TestPath {
        const char *env;
        const char *path;
    };

    static const TestPath test_paths[] = {
        { nullptr, "/usr/share/arduino" },
        { nullptr, "/usr/local/share/arduino" },
        { "HOME",  ".local/share/arduino" },
#ifdef __APPLE__
        { nullptr, "/Applications/Arduino.app/Contents/Java" }
#endif
    };

    for (const TestPath &test: test_paths) {
        if (test.env) {
            Span<const char> prefix = getenv(test.env);

            if (!prefix.len)
                continue;

            while (prefix.len && IsPathSeparator(prefix[prefix.len - 1])) {
                prefix.len--;
            }

            Fmt(out_cc, "%1%/%2%/%3", prefix, test.path, compiler);
        } else {
            Fmt(out_cc, "%1%/%2", test.path, compiler);
        }

        if (TestFile(out_cc.ptr, FileType::File)) {
            LogDebug("Found %1 compiler for Teensy: '%2'", name, out_cc.ptr);
            return;
        }
    }

    out_cc[0] = 0;
#endif
}

}
