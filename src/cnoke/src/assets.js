// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

// I'd prefer to keep these assets in separate files but until import attributes
// have landed and been in Node for long enough... it's more compatible this way!

const TOOLCHAINS = {
    linux_ia32: {
        system: "Linux",
        processor: "i386",
        triplet: "i686-linux-gnu",
        sysroot: "../../tools/cross/sysroots/debian_i386"
    },
    linux_x64: {
        system: "Linux",
        processor: "x86_64",
        triplet: "x86_64-linux-gnu",
        sysroot: "../../tools/cross/sysroots/debian_x64"
    },
    linux_armhf: {
        system: "Linux",
        processor: "arm",
        triplet: "arm-linux-gnueabihf",
        sysroot: "../../tools/cross/sysroots/debian_arm32"
    },
    linux_arm64: {
        system: "Linux",
        processor: "aarch64",
        triplet: "aarch64-linux-gnu",
        sysroot: "../../tools/cross/sysroots/debian_arm64"
    },
    linux_riscv64d: {
        system: "Linux",
        processor: "riscv64",
        triplet: "riscv64-linux-gnu",
        sysroot: "../../tools/cross/sysroots/debian_riscv64"
    },
    linux_loong64: {
        system: "Linux",
        processor: "loongarch64",
        triplet: "loongarch64-linux-gnu",
        sysroot: "../../tools/cross/sysroots/debian_loong64"
    },

    win32_ia32: {
        system: "Windows",
        processor: "X86",
        lib: "win-x86",
        flags: ["-DNODE_JS_LINK_FLAGS=/DELAYLOAD:node.exe;/SAFESEH:NO", "-A", "Win32"],

        triplet: "i386-pc-windows-msvc",
        linux: {
            sysroot: "../../tools/cross/sysroots/windows_i386",
            flags: ["-DNODE_JS_LINK_FLAGS=-Wl,/DELAYLOAD:node.exe,/SAFESEH:NO", "-DNODE_JS_DLLTOOL_MACHINE=i386"],
            variables: {
                CMAKE_DLLTOOL: "llvm-dlltool",
                CMAKE_RC_COMPILER: "llvm-rc",
                CMAKE_C_FLAGS: "-Xmicrosoft-windows-sys-root {{ sysroot }}",
                CMAKE_CXX_FLAGS: "-Xmicrosoft-windows-sys-root {{ sysroot }}",
                CMAKE_EXE_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/x86_64 -ldelayimp",
                CMAKE_STATIC_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/x86_64 -ldelayimp",
                CMAKE_SHARED_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/x86_64 -ldelayimp"
            }
        }
    },
    win32_x64: {
        system: "Windows",
        processor: "AMD64",
        lib: "win-x64",
        flags: ["-DNODE_JS_LINK_FLAGS=/DELAYLOAD:node.exe"],

        triplet: "x86_64-pc-windows-msvc",
        linux: {
            sysroot: "../../tools/cross/sysroots/windows_x64",
            flags: ["-DNODE_JS_LINK_FLAGS=-Wl,/DELAYLOAD:node.exe", "-DNODE_JS_DLLTOOL_MACHINE=i386:x86-64"],
            variables: {
                CMAKE_DLLTOOL: "llvm-dlltool",
                CMAKE_RC_COMPILER: "llvm-rc",
                CMAKE_C_FLAGS: "-Xmicrosoft-windows-sys-root {{ sysroot }}",
                CMAKE_CXX_FLAGS: "-Xmicrosoft-windows-sys-root {{ sysroot }}",
                CMAKE_EXE_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/x86_64 -ldelayimp",
                CMAKE_STATIC_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/x86_64 -ldelayimp",
                CMAKE_SHARED_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/x86_64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/x86_64 -ldelayimp"
            }
        }
    },
    win32_arm64: {
        system: "Windows",
        processor: "ARM64",
        lib: "win-arm64",
        flags: ["-DNODE_JS_LINK_FLAGS=/DELAYLOAD:node.exe;/SAFESEH:NO", "-A", "ARM64"],

        triplet: "aarch64-pc-windows-msvc",
        linux: {
            sysroot: "../../tools/cross/sysroots/windows_arm64",
            flags: ["-DNODE_JS_LINK_FLAGS=-Wl,/DELAYLOAD:node.exe,/SAFESEH:NO", "-DNODE_JS_DLLTOOL_MACHINE=arm64"],
            variables: {
                CMAKE_DLLTOOL: "llvm-dlltool",
                CMAKE_RC_COMPILER: "llvm-rc",
                CMAKE_C_FLAGS: "-Xmicrosoft-windows-sys-root {{ sysroot }}",
                CMAKE_CXX_FLAGS: "-Xmicrosoft-windows-sys-root {{ sysroot }}",
                CMAKE_EXE_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/aarch64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/aarch64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/aarch64 -ldelayimp",
                CMAKE_STATIC_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/aarch64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/aarch64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/aarch64 -ldelayimp",
                CMAKE_SHARED_LINKER_FLAGS: "-Wl,-libpath:{{ sysroot }}/VC/Tools/MSVC/Current/lib/aarch64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/ucrt/aarch64,-libpath:{{ sysroot }}/Kits/10/Lib/Current/um/aarch64 -ldelayimp"
            }
        }
    },

    darwin_x64: {
        system: "Darwin",
        processor: "x86_64",
        flags: ["-DNODE_JS_LINK_FLAGS=-undefined;dynamic_lookup", "-DCMAKE_OSX_ARCHITECTURES=x86_64"]
    },
    darwin_arm64: {
        system: "Darwin",
        processor: "arm64",
        flags: ["-DNODE_JS_LINK_FLAGS=-undefined;dynamic_lookup", "-DCMAKE_OSX_ARCHITECTURES=arm64"]
    },

    freebsd_x64: {
        system: "FreeBSD",
        processor: "amd64",
        triplet: "x86_64-unknown-freebsd",
        sysroot: "../../tools/cross/sysroots/freebsd_x64"
    },
    freebsd_arm64: {
        system: "FreeBSD",
        processor: "arm64",
        triplet: "aarch64-unknown-freebsd",
        sysroot: "../../tools/cross/sysroots/freebsd_arm64"
    }
};

const FIND_CNOKE_CMAKE = `# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
   CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(USE_UNITY_BUILDS ON CACHE BOOL "Use single-TU builds (aka. Unity builds)")
else()
    set(USE_UNITY_BUILDS OFF CACHE BOOL "Use single-TU builds (aka. Unity builds)")
endif()

function(add_node_addon)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES" \${ARGN})
    add_library(\${ARG_NAME} SHARED \${ARG_SOURCES} \${NODE_JS_SOURCES})
    target_link_node(\${ARG_NAME})
    set_target_properties(\${ARG_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
endfunction()

function(target_link_node TARGET)
    target_include_directories(\${TARGET} PRIVATE \${NODE_JS_INCLUDE_DIRS})
    if(NODE_JS_LINK_DEF)
        target_sources(\${TARGET} PRIVATE node.lib)
    endif()
    if(NODE_JS_LINK_LIB)
        target_link_libraries(\${TARGET} PRIVATE \${NODE_JS_LINK_LIB})
    endif()
    target_compile_options(\${TARGET} PRIVATE \${NODE_JS_COMPILE_FLAGS})
    if(NODE_JS_LINK_FLAGS)
        target_link_options(\${TARGET} PRIVATE \${NODE_JS_LINK_FLAGS})
    endif()
endfunction()

if(WIN32)
    function(create_import_lib OUTPUT SRC)
        if (MSVC)
            add_custom_command(OUTPUT \${OUTPUT}
                               COMMAND \${CMAKE_AR} \${CMAKE_STATIC_LINKER_FLAGS}
                                       /def:\${SRC} /out:"\${CMAKE_CURRENT_BINARY_DIR}/\${OUTPUT}"
                               WORKING_DIRECTORY \${CMAKE_CURRENT_SOURCE_DIR}
                               MAIN_DEPENDENCY \${SRC})
        elseif(DEFINED NODE_JS_DLLTOOL_MACHINE)
            add_custom_command(OUTPUT \${OUTPUT}
                               COMMAND \${CMAKE_DLLTOOL} -d \${SRC} -l "\${CMAKE_CURRENT_BINARY_DIR}/\${OUTPUT}" -m \${NODE_JS_DLLTOOL_MACHINE}
                               WORKING_DIRECTORY \${CMAKE_CURRENT_SOURCE_DIR}
                               MAIN_DEPENDENCY \${SRC})
        else()
            add_custom_command(OUTPUT \${OUTPUT}
                               COMMAND \${CMAKE_DLLTOOL} -d \${SRC} -l "\${CMAKE_CURRENT_BINARY_DIR}/\${OUTPUT}"
                               WORKING_DIRECTORY \${CMAKE_CURRENT_SOURCE_DIR}
                               MAIN_DEPENDENCY \${SRC})
        endif()
    endfunction()
endif()

if(NODE_JS_LINK_DEF)
    create_import_lib(node.lib \${NODE_JS_LINK_DEF})
    set(NODE_JS_LINK_LIB "\${CMAKE_CURRENT_BINARY_DIR}/node.lib")
endif()

if(USE_UNITY_BUILDS)
    function(enable_unity_build TARGET)
        cmake_parse_arguments(ARG "" "" "EXCLUDE" \${ARGN})

        get_target_property(sources \${TARGET} SOURCES)
        string(GENEX_STRIP "\${sources}" sources)

        set(unity_file_c "\${CMAKE_CURRENT_BINARY_DIR}/\${TARGET}_unity.c")
        set(unity_file_cpp "\${CMAKE_CURRENT_BINARY_DIR}/\${TARGET}_unity.cpp")
        file(REMOVE \${unity_file_c} \${unity_file_cpp})

        set(c_definitions "")
        set(cpp_definitions "")

        foreach(src \${sources})
            if (src IN_LIST ARG_EXCLUDE)
                continue()
            endif()

            get_source_file_property(language \${src} LANGUAGE)
            get_property(definitions SOURCE \${src} PROPERTY COMPILE_DEFINITIONS)
            if(IS_ABSOLUTE \${src})
                set(src_full \${src})
            else()
                set(src_full "\${CMAKE_CURRENT_SOURCE_DIR}/\${src}")
            endif()
            if(language STREQUAL "C")
                set_source_files_properties(\${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND \${unity_file_c} "#include \\"\${src_full}\\"\\n")
                if (definitions)
                    set(c_definitions "\${c_definitions} \${definitions}")
                endif()
            elseif(language STREQUAL "CXX")
                set_source_files_properties(\${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND \${unity_file_cpp} "#include \\"\${src_full}\\"\\n")
                if (definitions)
                    set(cpp_definitions "\${cpp_definitions} \${definitions}")
                endif()
            endif()
        endforeach()

        if(EXISTS \${unity_file_c})
            target_sources(\${TARGET} PRIVATE \${unity_file_c})
            if(c_definitions)
                set_source_files_properties(\${unity_file_c} PROPERTIES COMPILE_DEFINITIONS \${c_definitions})
            endif()
        endif()
        if(EXISTS \${unity_file_cpp})
            target_sources(\${TARGET} PRIVATE \${unity_file_cpp})
            if(cpp_definitions)
                set_source_files_properties(\${unity_file_cpp} PROPERTIES COMPILE_DEFINITIONS \${cpp_definitions})
            endif()
        endif()

        target_compile_definitions(\${TARGET} PRIVATE -DUNITY_BUILD=1)
    endfunction()
else()
    function(enable_unity_build TARGET)
    endfunction()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(amd64|x86_64)")
        foreach(lang C CXX)
            set(CMAKE_\${lang}_FLAGS_RELEASE "\${CMAKE_\${lang}_FLAGS_RELEASE} -mpopcnt -msse4.1 -msse4.2 -mssse3 -mcx16")
            set(CMAKE_\${lang}_FLAGS_RELWITHDEBINFO "\${CMAKE_\${lang}_FLAGS_RELWITHDEBINFO} -mpopcnt -msse4.1 -msse4.2 -mssse3 -mcx16")
        endforeach()
    endif()
endif()
`;

const WIN_DELAY_HOOK_C = `// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include <stdlib.h>
#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <delayimp.h>

static HMODULE node_dll;

static FARPROC WINAPI self_exe_hook(unsigned int event, DelayLoadInfo *info)
{
    if (event == dliStartProcessing) {
        node_dll = GetModuleHandleA("node.dll");
        if (!node_dll) {
            node_dll = GetModuleHandle(NULL);
        }
        return NULL;
    }

    if (event == dliNotePreLoadLibrary && !stricmp(info->szDll, "node.exe"))
        return (FARPROC)node_dll;

    return NULL;
}

#if defined(__MINGW32__)
PfnDliHook __pfnDliNotifyHook2 = self_exe_hook;
#else
const PfnDliHook __pfnDliNotifyHook2 = self_exe_hook;
#endif
`;

export {
    TOOLCHAINS,
    FIND_CNOKE_CMAKE,
    WIN_DELAY_HOOK_C
}
