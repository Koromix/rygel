// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#include "../libcc/libcc.hh"
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
