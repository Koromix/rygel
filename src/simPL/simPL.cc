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
#include "../libgui/libgui.hh"
#include "simulation.hh"
#include "view.hh"

static decltype(InitializeHumans) *InitializeHumans_;
static decltype(RunSimulationStep) *RunSimulationStep_;

#if !defined(NDEBUG) && !defined(__EMSCRIPTEN__)
#ifdef _WIN32
static HMODULE module_handle;
#else
static void *module_handle;
#endif

static bool LoadSimulationLibrary(const char *filename)
{
    // Check library time and unload if outdated
    if (module_handle) {
        static int64_t last_time = -1;

        FileInfo file_info;
        if (!StatFile(filename, &file_info))
            return false;

        if (last_time == file_info.modification_time)
            return true;
        last_time = file_info.modification_time;

#ifdef _WIN32
        FreeLibrary(module_handle);
#else
        dlclose(module_handle);
#endif
        module_handle = nullptr;
        InitializeHumans_ = nullptr;
        RunSimulationStep_ = nullptr;

        // Increase chance that the shared library is a complete file
        WaitForDelay(200);
    }

    // Load new library (or try to)
#ifdef _WIN32
    for (char c = 'A'; c <= 'D'; c++) {
        char copy_filename[4096];
        Fmt(copy_filename, "%1_%2.dll", filename, c);

        CopyFile(filename, copy_filename, FALSE);

        module_handle = LoadLibrary(copy_filename);
        if (!module_handle) {
            LogError("Cannot load library '%1'", filename);
            return false;
        }
    }

    const auto find_symbol = [&](const char *symbol_name) {
        return (void *)GetProcAddress(module_handle, symbol_name);
    };
#else
    module_handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    if (!module_handle) {
        LogError("Cannot load library '%1': %2", filename, dlerror());
        return false;
    }

    const auto find_symbol = [&](const char *symbol_name) {
        return (void *)dlsym(module_handle, symbol_name);
    };
#endif

    InitializeHumans_ = (decltype(InitializeHumans_))find_symbol("InitializeHumans");
    RunSimulationStep_ = (decltype(RunSimulationStep_))find_symbol("RunSimulationStep");
    DebugAssert(InitializeHumans_ && RunSimulationStep_);

    return true;
}
#endif

int main(int argc, char **argv)
{
#if !defined(NDEBUG) && !defined(__EMSCRIPTEN__)
    char module_filename[4096];
    {
        Span<const char> executable_path = GetApplicationExecutable();
        SplitStrReverse(executable_path, '.', &executable_path);
        Fmt(module_filename, "%1%2", executable_path, SHARED_LIBRARY_EXTENSION);
    }

    if (!LoadSimulationLibrary(module_filename))
        return 1;
#ifdef _WIN32
    DEFER { FreeLibrary(module_handle); };
#else
    DEFER { dlclose(module_handle); };
#endif
#else
    InitializeHumans_ = InitializeHumans;
    RunSimulationStep_ = RunSimulationStep;
#endif

    gui_Window window;
    if (!window.Init("simPL"))
        return 1;
    if (!window.InitImGui())
        return 1;

    HeapArray<Human> humans;

    InitializeHumans_(100, &humans);

    while (window.Prepare()) {
        RunSimulationStep_(humans.PrepareRewrite(), &humans);

        window.RenderImGui();
        window.SwapBuffers();

#if !defined(NDEBUG) && !defined(__EMSCRIPTEN__)
        while (!LoadSimulationLibrary(module_filename)) {
            WaitForDelay(1000);
        }
#endif
    }

    return 0;
}
