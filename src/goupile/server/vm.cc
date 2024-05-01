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
#include "vm.hh"
#include "src/core/wrap/jscore.hh"
#include "src/core/wrap/json.hh"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.h"

namespace RG {

static mz_zip_archive fs_zip;
static HashMap<const char *, Span<const uint8_t>> fs_map;
static BlockAllocator fs_alloc;

static bool LoadViewFile(const char *filename, Size max_len, Span<const uint8_t> *out_buf)
{
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

    fs_map.Set(filename, {});

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
        JSStringRef str = (JSStringRef)argv[0];
        JSStringGetUTF8CString(str, filename, RG_SIZE(filename));
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
    RG_ASSERT(ex);

    js_PrintValue(ctx, ex, nullptr, StdErr);
    PrintLn(StdErr);
}

static JSValueRef CallMethod(JSContextRef ctx, JSObjectRef obj, const char *method,
                             Span<const JSValueRef> args)
{
    JSValueRef func = JSObjectGetProperty(ctx, obj, js_AutoString(method), nullptr);

    RG_ASSERT(func);
    RG_ASSERT(JSValueIsObject(ctx, func) && JSObjectIsFunction(ctx, (JSObjectRef)func));

    JSValueRef ex = nullptr;
    JSValueRef ret = JSObjectCallAsFunction(ctx, (JSObjectRef)func, obj, args.len, args.ptr, &ex);

    if (!ret) {
        DumpException(ctx, ex);
        return nullptr;
    }

    return ret;
}

int RunVM(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *zip_filename = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st, R"(Usage: %!..+%1 vm <view_file>%!0)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        zip_filename = opt.ConsumeNonOption();
        if (!zip_filename) {
            LogError("Missing view ZIP filename");
            return 1;
        }
        opt.LogUnusedArguments();
    }

    // Open ZIP view
    if (!mz_zip_reader_init_file(&fs_zip, zip_filename, 0)) {
        LogError("Failed to open ZIP archive '%1': %2", zip_filename, mz_zip_get_error_string(fs_zip.m_last_error));
        return false;
    }
    RG_DEFER { mz_zip_reader_end(&fs_zip); };

    // Get packed server script
    HeapArray<char> vm_js;
    {
        const AssetInfo *asset = FindEmbedAsset("src/goupile/server/vm.js");
        RG_ASSERT(asset);

        StreamReader reader(asset->data, "<asset>", asset->compression_type);
        StreamWriter writer(&vm_js, "<memory>");

        if (!SpliceStream(&reader, -1, &writer))
            return false;
        RG_ASSERT(writer.Close());
    }

    // Prepare VM for JS execution
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    RG_DEFER { JSGlobalContextRelease(ctx); };

    // Run main script
    {
        JSValueRef ex = nullptr;
        JSValueRef ret = JSEvaluateScript(ctx, js_AutoString(vm_js), nullptr, nullptr, 1, &ex);

        if (!ret) {
            DumpException(ctx, ex);
            return 1;
        }
    }

    // Prepare our protected native API
    JSValueRef native;
    {
        JSObjectRef obj = JSObjectMake(ctx, nullptr, nullptr);

        js_ExposeFunction(ctx, obj, "getFile", GetFileData);

        native = (JSValueRef)obj;
    }

    // Create API instance
    JSObjectRef api;
    {
        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSValueRef app = JSObjectGetProperty(ctx, global, js_AutoString("app"), nullptr);
        JSValueRef construct = JSValueIsObject(ctx, app) ? JSObjectGetProperty(ctx, (JSObjectRef)app, js_AutoString("VmApi"), nullptr) : JSValueMakeUndefined(ctx);

        RG_ASSERT(JSValueIsObject(ctx, construct));
        RG_ASSERT(JSObjectIsFunction(ctx, (JSObjectRef)construct));

        JSValueRef args[] = {
            native
        };

        JSValueRef ex = nullptr;
        JSValueRef ret = JSObjectCallAsConstructor(ctx, (JSObjectRef)construct, RG_LEN(args), args, &ex);

        if (!ret) {
            DumpException(ctx, ex);
            return 1;
        }

        RG_ASSERT(JSValueIsObject(ctx, ret));
        api = (JSObjectRef)ret;
    }

    // Get data from stdin
    Span<const char> profile = {};
    Span<const char> payload = {};
    {
        json_Parser parser(StdIn, &temp_alloc);

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "profile") {
                if (parser.PeekToken() != json_TokenType::StartObject) {
                    LogError("Unexpected value type for profile");
                    return 1;
                }

                parser.PassThrough(&profile);
            } else if (key == "payload") {
                if (parser.PeekToken() != json_TokenType::StartObject) {
                    LogError("Unexpected value type for profile");
                    return 1;
                }

                parser.PassThrough(&payload);
            } else {
                LogError("Unexpected key '%1'", key);
                return 1;
            }
        }
        if (!parser.IsValid())
            return 1;

        if (!profile.len || !payload.len) {
            LogError("Missing profile or payload object");
            return 1;
        }
    }

    // Build app from main script
    JSValueRef app;
    {
        JSValueRef args[] = {
            JSValueMakeString(ctx, js_AutoString(profile))
        };

        app = CallMethod(ctx, api, "buildApp", args);
        if (!app)
            return 1;
    }

    // Run and transform user data
    JSValueRef transformed;
    {
        JSValueRef args[] = {
            app,
            JSValueMakeString(ctx, js_AutoString(profile)),
            JSValueMakeString(ctx, js_AutoString(payload))
        };

        transformed = CallMethod(ctx, api, "runData", args);
        if (!transformed)
            return 1;
    }

    // Return it back to the parent process
    if (!js_PrintValue(ctx, transformed, nullptr, StdOut))
        return 1;

    return 0;
}

}
