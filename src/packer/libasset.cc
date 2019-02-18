// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <time.h>
#endif

#include "../libcc/libcc.hh"
#include "libasset.hh"

asset_LoadStatus asset_AssetSet::LoadFromLibrary(const char *filename, const char *var_name)
{
    const Span<const asset_Asset> *lib_assets = nullptr;

#ifdef _WIN32
    // Check library time
    {
        StaticAssert(SIZE(FILETIME) == SIZE(last_time));

        FILETIME last_time_ft;
        memcpy(&last_time_ft, &last_time, SIZE(last_time));

        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &attr)) {
            LogError("Cannot stat file '%1'", filename);
            return asset_LoadStatus::Error;
        }

        if (attr.ftLastWriteTime.dwHighDateTime == last_time_ft.dwHighDateTime &&
                attr.ftLastWriteTime.dwLowDateTime == last_time_ft.dwLowDateTime)
            return asset_LoadStatus::Unchanged;

        memcpy(&last_time, &attr.ftLastWriteTime, SIZE(last_time));
    }

    HMODULE h = LoadLibrary(filename);
    if (!h) {
        LogError("Cannot load library '%1'", filename);
        return asset_LoadStatus::Error;
    }
    DEFER { FreeLibrary(h); };

    lib_assets = (const Span<const asset_Asset> *)GetProcAddress(h, var_name);
#else
    // Check library time
    {
        struct stat sb;
        if (stat(filename, &sb) < 0) {
            LogError("Cannot stat file '%1'", filename);
            return asset_LoadStatus::Error;
        }

#ifdef __APPLE__
        if (sb.st_mtimespec.tv_sec == last_time.tv_sec &&
                sb.st_mtimespec.tv_nsec == last_time.tv_nsec)
            return asset_LoadStatus::Unchanged;
        last_time = sb.st_mtimespec;
#else
        if (sb.st_mtim.tv_sec == last_time.tv_sec &&
                sb.st_mtim.tv_nsec == last_time.tv_nsec)
            return asset_LoadStatus::Unchanged;
        last_time = sb.st_mtim;
#endif
    }

    void *h = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    if (!h) {
        LogError("Cannot load library '%1': %2", filename, dlerror());
        return asset_LoadStatus::Error;
    }
    DEFER { dlclose(h); };

    lib_assets = (const Span<const asset_Asset> *)dlsym(h, var_name);
#endif
    if (!lib_assets) {
        LogError("Cannot find symbol '%1' in library '%2'", var_name, filename);
        return asset_LoadStatus::Error;
    }

    assets.Clear();
    alloc.ReleaseAll();
    for (const asset_Asset &asset: *lib_assets) {
        asset_Asset asset_copy;

        asset_copy.name = DuplicateString(asset.name, &alloc).ptr;
        uint8_t *data_ptr = (uint8_t *)Allocator::Allocate(&alloc, asset.data.len);
        memcpy(data_ptr, asset.data.ptr, (size_t)asset.data.len);
        asset_copy.data = {data_ptr, asset.data.len};
        asset_copy.compression_type = asset.compression_type;
        asset_copy.source_map = DuplicateString(asset.source_map, &alloc).ptr;

        assets.Append(asset_copy);
    }

    return asset_LoadStatus::Loaded;
}
