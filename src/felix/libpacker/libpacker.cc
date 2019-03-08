// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#include "../../libcc/libcc.hh"
#include "libpacker.hh"

pack_LoadStatus pack_AssetSet::LoadFromLibrary(const char *filename, const char *var_name)
{
    const Span<const pack_Asset> *lib_assets = nullptr;

    // Check library time
    {
        FileInfo file_info;
        if (!StatFile(filename, &file_info))
            return pack_LoadStatus::Error;

        if (last_time == file_info.modification_time)
            return pack_LoadStatus::Unchanged;
        last_time = file_info.modification_time;
    }

#ifdef _WIN32
    HMODULE h = LoadLibrary(filename);
    if (!h) {
        LogError("Cannot load library '%1'", filename);
        return pack_LoadStatus::Error;
    }
    DEFER { FreeLibrary(h); };

    lib_assets = (const Span<const pack_Asset> *)GetProcAddress(h, var_name);
#else
    void *h = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    if (!h) {
        LogError("Cannot load library '%1': %2", filename, dlerror());
        return pack_LoadStatus::Error;
    }
    DEFER { dlclose(h); };

    lib_assets = (const Span<const pack_Asset> *)dlsym(h, var_name);
#endif
    if (!lib_assets) {
        LogError("Cannot find symbol '%1' in library '%2'", var_name, filename);
        return pack_LoadStatus::Error;
    }

    assets.Clear();
    alloc.ReleaseAll();
    for (const pack_Asset &asset: *lib_assets) {
        pack_Asset asset_copy;

        asset_copy.name = DuplicateString(asset.name, &alloc).ptr;
        uint8_t *data_ptr = (uint8_t *)Allocator::Allocate(&alloc, asset.data.len);
        memcpy(data_ptr, asset.data.ptr, (size_t)asset.data.len);
        asset_copy.data = {data_ptr, asset.data.len};
        asset_copy.compression_type = asset.compression_type;
        asset_copy.source_map = DuplicateString(asset.source_map, &alloc).ptr;

        assets.Append(asset_copy);
    }

    return pack_LoadStatus::Loaded;
}

// This won't win any beauty or speed contest (especially when writing
// a compressed stream) but whatever.
Span<const uint8_t> pack_PatchVariables(pack_Asset &asset, Allocator *alloc,
                                        std::function<bool(const char *, StreamWriter *)> func)
{
    HeapArray<uint8_t> buf;
    buf.allocator = alloc;

    StreamReader reader(asset.data, nullptr, asset.compression_type);
    StreamWriter writer(&buf, nullptr, asset.compression_type);

    char c;
    while (reader.Read(1, &c) == 1) {
        if (c == '{') {
            char name[33] = {};
            Size name_len = reader.Read(1, &name[0]);
            Assert(name_len >= 0);

            bool valid = false;
            if (IsAsciiAlpha(name[0]) || name[0] == '_') {
                do {
                    Assert(reader.Read(1, &name[name_len]) >= 0);

                    if (name[name_len] == '}') {
                        name[name_len] = 0;
                        valid = func(name, &writer);
                        name[name_len++] = '}';

                        break;
                    } else if (!IsAsciiAlphaOrDigit(name[name_len]) && name[name_len] != '_') {
                        name_len++;
                        break;
                    }
                } while (++name_len < SIZE(name));
            }

            if (!valid) {
                writer.Write('{');
                writer.Write(name, name_len);
            }
        } else {
            writer.Write(c);
        }
    }
    Assert(!reader.error);

    Assert(writer.Close());
    return buf.Leak();
}
