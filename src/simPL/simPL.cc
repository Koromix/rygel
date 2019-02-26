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
#include "simPL.hh"
#include "simulate.hh"
#include "view.hh"
#include "../packer/libpacker.hh"
#include "../../lib/imgui/imgui.h"

extern const pack_Asset *const pack_asset_Roboto_Medium_ttf;

decltype(InitializeConfig) *InitializeConfig_;
decltype(InitializeHumans) *InitializeHumans_;
decltype(RunSimulationStep) *RunSimulationStep_;

BlockAllocator frame_alloc;

#ifdef SIMPL_ENABLE_HOT_RELOAD

#ifdef _WIN32
static HMODULE module_handle;
#else
static void *module_handle;
#endif

enum class LoadStatus {
    Loaded,
    Unchanged,
    Error
};

static LoadStatus LoadSimulationModule(const char *filename)
{
    // Check library time and unload if outdated
    if (module_handle) {
        static int64_t last_time = -1;

        FileInfo file_info;
        if (!StatFile(filename, &file_info))
            return LoadStatus::Error;

        if (last_time == file_info.modification_time)
            return LoadStatus::Unchanged;
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
            return LoadStatus::Error;
        }
    }

    const auto find_symbol = [&](const char *symbol_name) {
        return (void *)GetProcAddress(module_handle, symbol_name);
    };
#else
    module_handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    if (!module_handle) {
        LogError("Cannot load library '%1': %2", filename, dlerror());
        return LoadStatus::Error;
    }

    const auto find_symbol = [&](const char *symbol_name) {
        return (void *)dlsym(module_handle, symbol_name);
    };
#endif

    InitializeConfig_ = (decltype(InitializeConfig_))find_symbol("InitializeConfig");
    InitializeHumans_ = (decltype(InitializeHumans_))find_symbol("InitializeHumans");
    RunSimulationStep_ = (decltype(RunSimulationStep_))find_symbol("RunSimulationStep");
    DebugAssert(InitializeConfig_ && InitializeHumans_ && RunSimulationStep_);

    return LoadStatus::Loaded;
}

#endif

void Simulation::Reset()
{
    humans.Clear();
    alive_count = InitializeHumans_(config, &humans);
    iteration = 0;
}

int main(int, char **)
{
#ifdef SIMPL_ENABLE_HOT_RELOAD
    char module_filename[4096];
    {
        Span<const char> executable_path = GetApplicationExecutable();
        SplitStrReverse(executable_path, '.', &executable_path);
        Fmt(module_filename, "%1%2", executable_path, SHARED_LIBRARY_EXTENSION);
    }

    // The OS will unload this for us
    if (LoadSimulationModule(module_filename) == LoadStatus::Error)
        return 1;
#else
    InitializeConfig_ = InitializeConfig;
    InitializeHumans_ = InitializeHumans;
    RunSimulationStep_ = RunSimulationStep;
#endif

    ImFontAtlas font_atlas;
    {
        const pack_Asset &font = *pack_asset_Roboto_Medium_ttf;
        DebugAssert(font.data.len <= INT_MAX);

        ImFontConfig font_config;
        font_config.FontDataOwnedByAtlas = false;

        font_atlas.AddFontFromMemoryTTF((void *)font.data.ptr, (int)font.data.len,
                                        16, &font_config);
    }

    gui_Window window;
    if (!window.Init("simPL"))
        return 1;
    if (!window.InitImGui(&font_atlas))
        return 1;

    // More readable (for now)
    ImGui::StyleColorsLight();

    HeapArray<Simulation> simulations;

    while (window.Prepare()) {
        RenderMainMenu(&simulations);

        for (Size i = 0; i < simulations.len; i++) {
            Simulation *simulation = &simulations[i];

            if (RenderSimulationWindow(&simulations, i)) {
                if (simulation->alive_count && !simulation->pause) {
                    simulation->alive_count =
                        RunSimulationStep_(simulation->config, simulation->humans.PrepareRewrite(),
                                           &simulation->humans);
                    simulation->iteration++;
                }
            } else {
                SwapMemory(&simulations[i--], &simulations[simulations.len - 1], SIZE(Simulation));
                simulations.RemoveLast();
            }
        }

        window.RenderImGui();
        window.SwapBuffers();

        frame_alloc.ReleaseAll();

#ifdef SIMPL_ENABLE_HOT_RELOAD
        LoadStatus status = LoadStatus::Error;
        for (Size i = 1; status == LoadStatus::Error; i++) {
            status = LoadSimulationModule(module_filename);

            if (i >= 10) {
                LogError("Failed to load module too many times");
                return 1;
            }
        }

        if (status == LoadStatus::Loaded) {
            for (Simulation &simulation: simulations) {
                if (simulation.auto_reset) {
                    InitializeConfig_(simulation.config.count, simulation.config.seed,
                                      &simulation.config);
                    simulation.Reset();
                }
            }
        }
#endif
    }

    return 0;
}
